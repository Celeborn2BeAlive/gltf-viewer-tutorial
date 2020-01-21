#!/bin/bash

SCRIPT_DIR=`dirname "$0"`
source $SCRIPT_DIR/env.env

git clone https://github.com/KhronosGroup/glTF-Sample-Models $GLTF_MODELS_REPO_PATH