# OSPRay + Vive

A sample OBJ viewer for OSPRay using OpenVR SDK to work with the HTC Vive.

## Vive Module

The Vive module defines additional VR specific types for OSPRay such as the VR Camera and (in
the future) other types for foveated or distorted rendering and so on.

## Vive Sample App

The `ospray-vive` app uses the module and the [OpenVR SDK](https://github.com/ValveSoftware/openvr)
to render to the HMD. This app is a simple OBJ viewer which uses
[tinyobjloader](https://github.com/syoyo/tinyobjloader) to load.

