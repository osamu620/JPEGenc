# JPEGenc
An implementation of JPEG-1 Encoder in C++ with SIMD optimization

- For SIMD optimization, [google highway](https://github.com/google/highway) has been used.

## Dependency
- [google highway](https://github.com/google/highway) (>= 1.0.6) is required.

## Build from source
- Clone this repository with `--resursive` option.

```
git clone https://github.com/osamu620/JPEGenc.git --resursive
```

- Use cmake to generate build files. For example,

```
cd ${REPO}
cmake -B${BUILD_DIR} -DCMAKE_BUILD_TYPE=Release -G Ninja -DBUILD_TESTING=OFF
```

`${REPO}` is a root of cloned repository and `${BUILD_DIR}` is a build directory. `-DBUILD_TESTING=OFF` is for suppress generating highway's tests.

In `${BUILD_DIR}/bin`, you can find the example application, `jpenc`. `-h` shows the usage.

