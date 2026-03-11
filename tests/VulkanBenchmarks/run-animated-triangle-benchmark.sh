#!/usr/bin/env bash
set -euo pipefail

backend="cpu"
seconds="10"
scene="color"

for arg in "$@"; do
    case "$arg" in
        --backend=cpu|--backend=cuda)
            backend="${arg#*=}"
            ;;
        --seconds=*)
            seconds="${arg#*=}"
            ;;
        --scene=color|--scene=texture)
            scene="${arg#*=}"
            ;;
        *)
            echo "unknown argument: $arg" >&2
            exit 1
            ;;
    esac
done

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"

if [[ "$backend" == "cuda" ]]; then
    build_dir="${repo_root}/build-cuda-bootstrap"
    cmake_args=(
        -DSWIFTSHADER_BUILD_BENCHMARKS=ON
        -DSWIFTSHADER_BUILD_TESTS=ON
        -DSWIFTSHADER_ENABLE_GPU_BACKEND=ON
        -DSWIFTSHADER_GPU_USE_CUDA=ON
    )
else
    build_dir="${repo_root}/build-benchmark-cpu"
    cmake_args=(
        -DSWIFTSHADER_BUILD_BENCHMARKS=ON
        -DSWIFTSHADER_BUILD_TESTS=ON
        -DSWIFTSHADER_ENABLE_GPU_BACKEND=OFF
    )
fi

cmake -S "${repo_root}" -B "${build_dir}" "${cmake_args[@]}"
cmake --build "${build_dir}" --target animated-triangle-benchmark --parallel "$(nproc)"

cd "${build_dir}"
exec ./animated-triangle-benchmark \
    "--seconds=${seconds}" \
    "--backend-label=${backend}" \
    "--scene=${scene}"
