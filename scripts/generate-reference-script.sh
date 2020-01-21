#!/bin/bash

SCRIPT_DIR=`dirname "$0"`
SRC_DIR=$SCRIPT_DIR/..
source $SCRIPT_DIR/env.env

for d in ${GLTF_MODELS_PATH}/*/ ; do
  MODEL_NAME=`basename $d`
  PATH_TO_GLTF_FILE=$MODEL_NAME/'glTF'/$MODEL_NAME.gltf
  echo "gltf-viewer viewer \$GLTF_MODELS_PATH/$PATH_TO_GLTF_FILE --output \$REF_IMG_DIR/$MODEL_NAME.png --output-args \$REF_IMG_DIR/$MODEL_NAME.args"
done