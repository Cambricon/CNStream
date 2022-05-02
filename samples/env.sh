#!/bin/bash

# Current file's directory
ENV_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)

# CNStream project directory
CNSTREAM_DIR=${ENV_DIR}/..

# sample directory which includes several demos
SAMPLES_DIR=${CNSTREAM_DIR}/samples

# the models used by demos, downloaded from offline model zoo if not exist
MODELS_DIR=${CNSTREAM_DIR}/data/models

# JSON configuration files
CONFIGS_DIR=${SAMPLES_DIR}/cns_launcher/configs
