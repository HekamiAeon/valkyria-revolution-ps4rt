// SPDX-License-Identifier: GPL-2.0-or-later
// Camera candidate observed in Valkyria's 3936-byte geometry VS constants.
// Validate the perspective/rigid-transform structure before consuming it.
#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace Vulkan::ValkyriaCamera {
struct Camera {
    std::array<float,12> view{}, inverse_view{};
    std::array<float,4> projection{};
};

inline bool Read(std::span<const float> data, Camera& result) {
    if (data.size()!=3936/sizeof(float)) return false;
    for (size_t i=4;i<32;++i) if (!std::isfinite(data[i])) return false;
    // Row-major right-handed perspective: x/y scale, clip w=-view z.
    // Screen geometry uses a symmetric projection; reject UI/orthographic data.
    constexpr size_t zeros[]={5,6,7,8,10,11,12,13,16,17,19};
    for (const auto i:zeros) if (std::abs(data[i])>1e-5f) return false;
    if (data[4]<0.05f || data[4]>100.f || data[9]<0.05f || data[9]>100.f ||
        std::abs(data[18]+1.f)>1e-5f || data[14]>=-1.f || data[15]>=0.f) return false;
    const float near_plane=data[15]/data[14];
    const float far_plane=data[15]/(data[14]+1.f);
    if (!(near_plane>0.001f && far_plane>near_plane && std::isfinite(far_plane))) return false;
    // The camera pose must be rigid. This rejects scaled data and prevents
    // using a transpose as an inverse for a non-orthogonal matrix.
    for (size_t r=0;r<3;++r) for (size_t c=0;c<3;++c) {
        float dot=0;
        for (size_t k=0;k<3;++k) dot+=data[20+r*4+k]*data[20+c*4+k];
        if (std::abs(dot-(r==c ? 1.f:0.f))>0.003f) return false;
    }
    const float determinant=
        data[20]*(data[25]*data[30]-data[26]*data[29])-
        data[21]*(data[24]*data[30]-data[26]*data[28])+
        data[22]*(data[24]*data[29]-data[25]*data[28]);
    if (std::abs(determinant-1.f)>0.003f) return false;
    Camera camera;
    // Offset 20 is camera-to-world, not world-to-camera. Adjacent-frame image
    // reprojection distinguishes these despite both satisfying the rigid checks.
    // Existing motion shaders use forward-positive view z: inverse_view = pose*S,
    // where S flips camera Z. Its rigid inverse is S*inverse(pose).
    for (size_t r=0;r<3;++r) for (size_t c=0;c<4;++c)
        camera.inverse_view[r*4+c]=data[20+r*4+c]*(c==2 ? -1.f:1.f);
    for (size_t r=0;r<3;++r) {
        for (size_t c=0;c<3;++c) camera.view[r*4+c]=camera.inverse_view[c*4+r];
        camera.view[r*4+3]=0;
        for (size_t c=0;c<3;++c)
            camera.view[r*4+3]-=camera.view[r*4+c]*camera.inverse_view[c*4+3];
    }
    camera.projection={data[4],data[9],-data[14],data[15]};
    result=camera;
    return true;
}
} // namespace Vulkan::ValkyriaCamera
