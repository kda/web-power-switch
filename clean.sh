#!/bin/bash

rm -rf build
find subprojects -mindepth 1 ! -name wrapdb.json -delete
