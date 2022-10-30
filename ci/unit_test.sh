#!/bin/bash

set -euxo pipefail

cd esp32-src/clock/main/tests
make
