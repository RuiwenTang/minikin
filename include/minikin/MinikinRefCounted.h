/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Base class for reference counted objects in Minikin

#ifndef MINIKIN_REF_COUNTED_H
#define MINIKIN_REF_COUNTED_H

#include <type_traits>

namespace android {

 class [[deprecated]] MinikinRefCounted {
public:
    void RefLocked() { mRefcount_++; }
    void UnrefLocked() {
        if (--mRefcount_ == 0) {
            delete this;
        }
    }

    // These refcount operations take the global lock.
    void Ref();
    void Unref();

    MinikinRefCounted() : mRefcount_(1) {}

    virtual ~MinikinRefCounted(){};

private:
    int mRefcount_;
};

// An RAII container for reference counted objects.
// Note: this is only suitable for clients which are _not_ holding the global lock.
template <typename T>
class MinikinAutoUnref {
public:
    explicit MinikinAutoUnref(T* obj) : mObj(obj) {}
    ~MinikinAutoUnref() { mObj->Unref(); }
    T& operator*() const { return *mObj; }
    T* operator->() const { return mObj; }
    T* get() const { return mObj; }

private:
    T* mObj;
};

template <class T,
          typename V =
                  typename std::enable_if<std::is_base_of<MinikinRefCounted, T>::value, void>::type>
class MinikinAutoRefUnRef {
public:
    explicit MinikinAutoRefUnRef(T* obj) : mObj(obj) {
        if (obj) obj->Ref();
    }
    ~MinikinAutoRefUnRef() {
        if (mObj) mObj->Unref();
    }
    MinikinAutoRefUnRef(const MinikinAutoRefUnRef& other) : mObj(other.mObj) {
        if (mObj) mObj->Ref();
    }
    MinikinAutoRefUnRef& operator=(const MinikinAutoRefUnRef& other) {
        if (this != &other) {
            if (mObj) {
                mObj->Unref();
            }
            mObj = other.mObj;
            if (mObj) mObj->Ref();
        }
        return *this;
    }

    MinikinAutoRefUnRef& operator=(T* obj) {
        if (obj != mObj) {
            if (mObj) {
                mObj->Unref();
            }
            mObj = obj;
            if (mObj) {
                mObj->Ref();
            }
        }
        return *this;
    }
    T& operator*() const { return *mObj; }
    T* operator->() const { return mObj; }
    T* get() const { return mObj; }

private:
    T* mObj;
};

} // namespace android

#endif // MINIKIN_REF_COUNTED_H