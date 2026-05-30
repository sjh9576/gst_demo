#!/bin/bash

IMAGE_NAME="pytorch-2-11-dev"
TAG="latest"

LOCAL_DIR=$(realpath ..)
CONTAINER_DIR="/workspace"

docker run --gpus all -it --rm \
    -v "$LOCAL_DIR:$CONTAINER_DIR" \
    --name "pytorch-2-11-container" \
    --shm-size=16g \
    --net=host \
    --workdir "$CONTAINER_DIR" \
    -e DISPLAY=$DISPLAY \
    -e WAYLAND_DISPLAY=$WAYLAND_DISPLAY \
    -e XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR \
    -e PULSE_SERVER=$PULSE_SERVER \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v /mnt/wslg:/mnt/wslg \
    $IMAGE_NAME:$TAG