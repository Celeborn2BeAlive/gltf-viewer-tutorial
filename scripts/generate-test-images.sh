#!/bin/bash

if [ -z "$1" ]
then
  echo "No argument supplied. Please provide the name of the reference folder (commit tag of the version used to generate reference images)"
  exit 1
fi

SCRIPT_DIR=`dirname "$0"`
SRC_DIR=$SCRIPT_DIR/..
source $SCRIPT_DIR/env.env

COMMIT_SLUG="$1"
REF_IMG_DIR=$REF_IMG_DIR/$COMMIT_SLUG

TEST_IMG_DIR=$TEST_IMG_DIR/`date +%Y-%m-%d-%H-%m-%S`

mkdir -p $TEST_IMG_DIR

for f in ${REF_IMG_DIR}/*.png ; do
  MODEL_NAME=`basename $f .png`
  PATH_TO_GLTF_FILE=$MODEL_NAME/'glTF'/$MODEL_NAME.gltf
  PATH_TO_ARGS=${REF_IMG_DIR}/${MODEL_NAME}.args
  gltf-viewer viewer $GLTF_MODELS_PATH/$PATH_TO_GLTF_FILE `cat $PATH_TO_ARGS` --output $TEST_IMG_DIR/$MODEL_NAME.png --output-args $TEST_IMG_DIR/$MODEL_NAME.args
done