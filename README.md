# 🍉 engine

## TODO:
1. Reorg
  * ~"melon" namespace, replace c style prefixes, rename "tz_" prefix~
  * ~"src" -> "tests" containing actual applications building w/ melon~
  * ~"lib" -> "src"~
2. Graphics backend
  * OpenGL backend
  * Vulkan backend
  * Command buffers
  * Uniforms
  * Textures
  * Render passes
  * C bindings?

## Design goals
  * Default 2D, but 3D capable
  * Simple, but scalable
  * Modular, so that some parts can be pulled out and used piecemeal
  * Cute

## Style
  * Syntax style is enforced by .clang-format
  * General motivation is to be C-style, but using a subset of C++ features
  * Make sparse use of templates
  * OOP is not dogma: keep things readable, simple, and where possible, linear
  * Zero is initialization

## Third party dependencies
  * GLFW (http://www.glfw.org/) included as a submodule
  * tinycthread (https://tinycthread.github.io/) included as a submodule
