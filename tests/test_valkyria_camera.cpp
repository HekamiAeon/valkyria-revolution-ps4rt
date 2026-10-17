// SPDX-License-Identifier: GPL-2.0-or-later
#undef NDEBUG
#include <algorithm>
#include <cassert>
#include <limits>
#include <cstdio>
#include "video_core/renderer_vulkan/valkyria_camera.h"

int main() {
    using namespace Vulkan::ValkyriaCamera;
    std::array<float,984> data{};
    data[4]=1.5f; data[9]=data[4]*16.f/9.f;
    constexpr float near_plane=1.f,far_plane=10000.f;
    data[14]=-far_plane/(far_plane-near_plane);
    data[15]=-near_plane*far_plane/(far_plane-near_plane); data[18]=-1;
    const float c=std::cos(0.3f),s=std::sin(0.3f);
    const std::array<float,12> pose={c,0,s,3, 0,1,0,-2, -s,0,c,7};
    std::copy(pose.begin(),pose.end(),data.begin()+20);
    Camera camera;
    assert(Read(data,camera));
    assert(camera.projection[2]>1 && camera.projection[3]<0);
    // View and inverse compose to identity, including a translated/rotated view
    // and the right-handed to forward-positive-z conversion.
    for (size_t r=0;r<3;++r) for (size_t col=0;col<4;++col) {
        float value=col==3 ? camera.view[r*4+3]:0.f;
        for (size_t k=0;k<3;++k) value+=camera.view[r*4+k]*camera.inverse_view[k*4+col];
        assert(std::abs(value-(r==col ? 1.f:0.f))<1e-5f);
    }
    // A point ten units in front of a translated/rotated camera must land in
    // view space at (1,2,+10), independently of the pose's world location.
    const std::array<float,3> world={3+c-10*s, 0, 7-s-10*c};
    for (size_t r=0;r<3;++r) {
        float value=camera.view[r*4+3];
        for (size_t k=0;k<3;++k) value+=camera.view[r*4+k]*world[k];
        assert(std::abs(value-std::array{1.f,2.f,10.f}[r])<1e-5f);
    }
    for (float z:{-2.f,-20.f,-200.f}) {
        const float old_depth=(data[14]*z+data[15])/-z;
        const float new_z=-z;
        const float new_depth=camera.projection[2]+camera.projection[3]/new_z;
        assert(std::abs(old_depth-new_depth)<1e-6f);
        assert(std::abs((camera.projection[3]/(new_depth-camera.projection[2])-new_z)/new_z)<0.001f);
    }
    // Move an otherwise unrotated camera one unit right. A stationary point at
    // (0,0,-10) moves left by 144 pixels at width 1920 and projection scale 1.5.
    auto translated=data;
    std::fill(translated.begin()+20,translated.begin()+32,0.f);
    translated[20]=translated[25]=translated[30]=1;
    Camera origin, right;
    assert(Read(translated,origin));
    translated[23]=1;
    assert(Read(translated,right));
    const auto screen_x=[](const Camera& cam) {
        const float x=cam.view[3]-10*cam.view[2];
        const float z=cam.view[11]-10*cam.view[10];
        return 960.f*(1+cam.projection[0]*x/z);
    };
    assert(std::abs(screen_x(origin)-960.f)<1e-5f);
    assert(std::abs(screen_x(right)-816.f)<1e-5f);
    assert(std::abs((screen_x(origin)-screen_x(right))-144.f)<1e-5f);
    const auto valid=data;
    const auto reject=[&] {
        Camera untouched{}; untouched.projection[0]=77;
        assert(!Read(data,untouched) && untouched.projection[0]==77);
        data=valid;
    };
    data[4]=0; reject();
    data[18]=0; reject(); // orthographic/UI
    data[14]=-1; reject(); // unsupported infinite projection
    data[15]=1; reject(); // wrong depth convention
    data[6]=0.2f; reject(); // asymmetric projection
    data[20]*=2; reject(); // scaled object transform
    data[23]=std::numeric_limits<float>::quiet_NaN(); reject();
    for (size_t i=20;i<32;++i) data[i]=0;
    reject(); // singular transform
    for (size_t i=20;i<24;++i) data[i]=-data[i];
    reject(); // reflected transform
    assert(!Read(std::span<const float>(data).first(32),camera));
    assert(!Read(std::span<const float>(data).first(983),camera));
    std::puts("PASS: Valkyria camera pose, projected world point/motion, inverse, depth and invalid inputs");
}
