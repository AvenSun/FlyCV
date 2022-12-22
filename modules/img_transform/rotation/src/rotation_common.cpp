// Copyright (c) 2021 FlyCV Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "modules/img_transform/rotation/include/rotation_common.h"

#include "modules/core/parallel/interface/parallel.h"

G_FCV_NAMESPACE1_BEGIN(g_fcv_ns)

template<typename T>
void transpose_c(const T* src,
        int src_h,
        int src_w,
        int sc,
        int sstep,
        T* dst,
        int dstep) {
    T *dst_col = nullptr;
    int i = 0, j = 0, k = 0;
    for (; i < src_h; i++) {
        dst_col = dst;
        for (j = 0; j < src_w; j++) {
            for (k = 0; k < sc; k++) {
                dst_col[k] = src[j * sc + k];
            }
            dst_col += dstep;
        }

        dst += sc;
        src += sstep;
    }
}

template<typename T>
void transpose_c_random(const T* src,
        Range range, 
        int src_w,
        int sc,
        int sstep,
        T* dst,
        int dstep) {
    const T* ptr_cur_src = src + range.start() * sstep;
    T* ptr_cur_dst_base = dst + range.start() * sc;

    for (int i = range.start(); i < range.end(); ++i) {
        T* ptr_cur_dst = ptr_cur_dst_base;
        for (int j = 0; j < src_w; j++) {
            for (int k = 0; k < sc; k++) {
                ptr_cur_dst[k] = ptr_cur_src[j * sc + k];
            }
            ptr_cur_dst += dstep;
        }
        ptr_cur_dst_base += sc;
        ptr_cur_src += sstep;
    }
}

class RotationCommonParallelTask : public ParallelTask {
public:
    RotationCommonParallelTask(
            int sstep,
            const void* src_ptr,
            int dstep,
            void* dst_ptr,
            int src_w, 
            int channel, 
            FCVImageType image_type)
            : _sstep(sstep),
            _src_ptr(src_ptr),
            _dstep(dstep),
            _dst_ptr(dst_ptr),
            _src_w(src_w),
            _channel(channel),  
            _image_type(image_type) {}

    void operator() (const Range& range) const override {
        switch (_image_type) {
        case FCVImageType::GRAY_U8:
        case FCVImageType::PKG_RGB_U8:
        case FCVImageType::PKG_BGR_U8:
        case FCVImageType::PKG_RGBA_U8:
        case FCVImageType::PKG_BGRA_U8:
            transpose_c_random<uint8_t>((const uint8_t*)_src_ptr, range, _src_w,
                    _channel, _sstep, (uint8_t*)_dst_ptr, _dstep);
            break;
        case FCVImageType::GRAY_F32:
        case FCVImageType::PKG_RGB_F32:
        case FCVImageType::PKG_BGR_F32:
        case FCVImageType::PKG_RGBA_F32:
        case FCVImageType::PKG_BGRA_F32:
            transpose_c_random<float>((const float*)_src_ptr, range, _src_w, 
                    _channel, _sstep, (float*)_dst_ptr, _dstep);
            break;
        default:
            LOG_ERR("transpose type not support yet!");
            break;
        };
    }
private:
    int _sstep;
    const void* _src_ptr;
    int _dstep;
    void* _dst_ptr;
    int _src_w;
    int _channel;
    FCVImageType _image_type;
};

static void transpose_common_multi_thread(const Mat& src, Mat& dst) {
    int src_w = src.width();
    int src_h = src.height();
    int channel = src.channels();
    int sstep = src_w * channel;
    int dstep = dst.width() * channel;
    const void* src_ptr = src.data();
    void* dst_ptr = dst.data();

    RotationCommonParallelTask task(sstep, src_ptr, dstep, dst_ptr, src_w, channel, src.type());
    parallel_run(Range(0, src_h), task);
}

int transpose_common(const Mat& src, Mat& dst) {
    const int src_w = src.width();
    const int src_h = src.height();
    const int dst_w = dst.width();
    const int dst_h = dst.height();
    if ((src_w != dst_h) || (src_h != dst_w)) {
        LOG_ERR("size of input or output is not match!");
        return -1;
    }
    transpose_common_multi_thread(src, dst);
    return 0;
}

G_FCV_NAMESPACE1_END()
