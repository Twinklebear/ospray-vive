# OSPRay + Vive

A sample OBJ viewer for OSPRay using OpenVR SDK to work with the HTC Vive.

## Building

First clone the module into OSPRay's `modules/` directory.

```
cd $OSPRAY/modules
git git@github.com:Twinklebear/ospray-vive.git
```

Then cd back to your OSPRay build directory and run CMake with the module enabled
via `-DOSPRAY_MODULE_VIVE=ON`. SDL2 and the OpenVR SDK are required to build.
If these libraries are installed in non-standard locations (or you're on Windows)
you can specify the root directories of each library when running CMake.
For example:

```
cmake <other OSPRay params> \
	-DOSPRAY_MODULE_VIVE=ON \
	-DSDL2_DIR=<..> -DOPENVR_DIR=<..>
```

## Vive Module

The Vive module defines additional VR specific types for OSPRay such as
the VR Camera and (in the future) other types for foveated or distorted
rendering and so on.

## Vive Sample App

The `ospray-vive` app uses the module and
the [OpenVR SDK](https://github.com/ValveSoftware/openvr)
to render to the HMD. This app is a simple OBJ viewer which uses
[tinyobjloader](https://github.com/syoyo/tinyobjloader) to load OBJ files
and renders them using the `raycast_Ns` renderer to render them. For example:

```
./ospray-vive <path to model>
```

