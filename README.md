# Javelin

<table>
    <tr>
        <td><img src="room_pt.png"></td>
        <td><img src="room_mlt.png"></td>
    </tr>
    <tr>
        <td>Path Tracing</td>
        <td>Metropolis Light Transport</td>
    </tr>
</table>

## Overview

In this project we implement a modified version of Veach and Guibas' original 1997 Metropolis Light Transport algorithm. The renderer loads scenes from `.glb` files and supports both a unidirectional path tracer and the MLT algorithm. A Qt-based user interface will provide scene and renderer controls.

## Building

This project requires Qt 6.10, Conan 2.25, CMake version 3.25, and a compiler with support for C++23.

```
conan install .
cmake --preset conan-release
cmake --build --preset conan-release
```

To enable debugging symbols:
```
conan install . -s "&:build_type=RelWithDebInfo"
cmake --preset conan-relwithdebinfo
cmake --build --preset conan-relwithdebinfo
```

## Profiling with Tracy

![Profiling with Tracy](tracy.png)

Our code has been instrumented with the help of [Tracy](https://github.com/wolfpld/tracy) to assist in identifying performance bottlenecks.

To enable Tracy profiling, run:
```
conan install . -s "&:build_type=RelWithDebInfo" -o "&:enable_profiling=True"
cmake --fresh --preset conan-relwithdebinfo
cmake --build --preset conan-relwithdebinfo
```

Once built, run the application as usual. Profiling of instrumented code will be
enabled automatially. In order to profile using sampling, the application needs
to be run with administrative privileges.

Finally, install [Tracy Profiler 0.13.1](https://github.com/wolfpld/tracy/releases/tag/v0.13.1) and run it to connect to the application.

## Implementation
We use a bounding volume heirarchy with the surface area heuristic to speed up ray-triangle intersections. Our code is also multithreaded by default (use `-j` option to set the number of threads used). Implementing Metropolis Light Transport demanded a deep and thourough understanding of the theoretical background and the implementation details which drive the algorithm. The paper that this project was based on is given here: [Veach & Guibas](https://graphics.stanford.edu/papers/metro/metro.pdf).

## Caveats
Note that the Metropolis Light Transport shines best on scenes with difficult lighting (such as the provided `room_far.glb` scene). For scenes with many large light sources, the path tracer may perform better and this is to be expected. That being said, our MLT implementation should still stay competitive with the path tracing implementation. Additionally, because our MLT implementation is unidirectional it does not perform as well with glass and caustics as the implementation presented by Veach and Guibas. 

## Attribution

The scenes `media/room_close.glb` and `media/room_far.glb` use select assets from the [Living room Asset Pack](https://sketchfab.com/3d-models/living-room-asset-pack-eafcacc7dc8643d68b435b177d049567) by [shadowdraak](https://sketchfab.com/shadowdraak), licensed under [Creative Commons Attribution (CC BY 4.0)](https://creativecommons.org/licenses/by/4.0/).


The scene `media/shaderball.glb` uses the [Standard Shader Ball](https://github.com/usd-wg/assets/tree/main/full_assets/StandardShaderBall) by the following contributors,

- Geometry and textures: Chris Rydalch,
- Specification and validation: André Mazzone,
- Original scene, inspiration and consultation: Thomas Anagnostou,

licensed under [Creative Commons Attribution (CC BY 4.0)](https://creativecommons.org/licenses/by/4.0/).
