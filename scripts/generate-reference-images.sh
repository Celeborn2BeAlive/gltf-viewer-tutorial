#!/bin/bash

SCRIPT_DIR=`dirname "$0"`
SRC_DIR=$SCRIPT_DIR/..
source $SCRIPT_DIR/env.env

COMMIT_SLUG=`git rev-parse HEAD`
REF_IMG_DIR=$REF_IMG_DIR/$COMMIT_SLUG

mkdir -p $REF_IMG_DIR

source $SCRIPT_DIR/generate-reference-images-cmd.sh