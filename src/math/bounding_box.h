#include "../containers/array.h"
#include "vector3.h"
#include "matrix4.h"

namespace nslib
{

template<class T>
struct bounding_box
{
    enum box_face
    {
        f_none,
        f_bottom,
        f_top,
        f_left,
        f_right,
        f_back,
        f_front
    };

    bounding_box(const array<vector3<T>> &verts_ = array<vector3<T>>(), const matrix4<T> &tform_=matrix4<T>())
    {
        calculate(verts_, tform_);
    }

    bounding_box(const vector3<T> &min_, const vector3<T> &max_): min(min_), max(max_)
    {
        update_verts();
    }

    ~bounding_box()
    {}

    void calculate(const array<vector3<T>> &verts_, const matrix4<T> &tform_ = matrix4<T>())
    {
        clear();
        extend(verts_, tform_);
    }

    vector3<T> center(const box_face &face_ = f_none)
    {
        vector3<T> center = (max - min) * 0.5;
        switch (face_)
        {
        case (f_none):
            break;
        case (f_bottom):
            center.z = min.z;
            break;
        case (f_top):
            center.z = max.z;
            break;
        case (f_left):
            center.x = min.x;
            break;
        case (f_right):
            center.x = max.x;
            break;
        case (f_back):
            center.y = min.y;
            break;
        case (f_front):
            center.y = max.y;
            break;
        }
        return center;
    }

    void clear()
    {
        min = vector3<T>();
        max = vector3<T>();
        for (sizet i = 0; i < 8; ++i)
            verts[i] = vector3<T>();
    }

    void extend(const array<vector3<T>> &verts_, const matrix4<T> &tform_ = matrix4<T>())
    {
        if (!verts_.empty())
        {
            min = verts_[0];
            max = min;
        }
        
        for (sizet i = 0; i < verts_.size(); ++i)
        {
            vector3<T> tvert = (tform_ * vector4<T>(verts_[i], 1.0f)).vec3();

            // Find maximum of each dimension
            max.maximize(tvert);

            // Find minimum of each dimension
            min.minimize(tvert);
        }
        update_verts();
    }

    T volume()
    {
        vector3<T> diff = (max - min).abs();
        return diff.x * diff.y * diff.z;
    }

    void update_verts()
    {
        verts[0] = min;
        verts[1] = vector3<T>(max.x, min.y, min.z);
        verts[2] = vector3<T>(min.x, max.y, min.z);
        verts[3] = vector3<T>(max.x, max.y, min.z);
        verts[4] = vector3<T>(min.x, min.y, max.z);
        verts[5] = vector3<T>(max.x, min.y, max.z);
        verts[6] = vector3<T>(min.x, max.y, max.z);
        verts[7] = max;
    }

    vector3<T> min;
    vector3<T> max;
    vector3<T> verts[8];
};

using cbbox = bounding_box<char>;
using c16bbox = bounding_box<c16>;
using c32bbox = bounding_box<c32>;
using cwbbox = bounding_box<wchar>;
using s8bbox = bounding_box<s8>;
using s16bbox = bounding_box<s16>;
using sbbox = bounding_box<s32>;
using s64bbox = bounding_box<s64>;
using ui8bbox = bounding_box<u8>;
using ui16bbox = bounding_box<u16>;
using uibbox = bounding_box<u32>;
using ui64bbox = bounding_box<u64>;
using bbox = bounding_box<f32>;
using f64bbox = bounding_box<f64>;
using f128bbox = bounding_box<f128>;

} // namespace nslib
