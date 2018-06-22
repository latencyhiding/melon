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

## Design goals
  * Simple, but scalable
  * Cute

## Style
  * Syntax style is enforced by .clang-format
  * General motivation is to be C-style, but using a subset of C++ features

## Third party dependencies
  * GLFW (http://www.glfw.org/) included as a submodule
  * tinycthread (https://tinycthread.github.io/) included as a submodule
