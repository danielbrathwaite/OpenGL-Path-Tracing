# Ray Tracing in OpenGL

This is an implementation of the path tracing algorithm using `C++` and `OPENGL`.

## Getting Started

1. Clone this repository.
2. Generate your scene data in Blender, and output to `.obj` file format. Make sure to triangulate all surfaces. Drop your `.obj` file in the project directory.
3. Run `main.cpp` with your `.obj` file name as an argument.

## Built With

* [OpenGL](https://www.opengl.org/) - The GPGPU API used

## Example Output

![rtximg](https://github.com/danielbrathwaite/OpenGL-Path-Tracing/assets/93694908/3049e281-98f6-4f59-a86e-005568b32542)

| Normal Buffer | Color Buffer | Depth Buffer |
| ------------- | ------------ | ------------ |
|![normalbuffer](https://github.com/danielbrathwaite/OpenGL-Path-Tracing/assets/93694908/bc44e018-051e-4fab-b7ff-ec236f52cfc9) | ![colorbuffer](https://github.com/danielbrathwaite/OpenGL-Path-Tracing/assets/93694908/127022bf-32e2-4eea-b4a0-64938351b498) | ![depthbuffer](https://github.com/danielbrathwaite/OpenGL-Path-Tracing/assets/93694908/6127618e-b7a0-4eb1-9450-f84cb099b01d) |




## Authors

* **Daniel Brathwaite**

## Acknowledgments

* [Sebastian Lague](https://www.youtube.com/watch?v=Qz0KTGYJtUk)
* [Peter Shirley](https://raytracing.github.io/)