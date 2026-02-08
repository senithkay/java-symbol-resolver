#!/bin/bash

# Set your Java installation path here
JAVA_PATH="/usr/lib/jvm/java-17-openjdk-arm64"

# Export JAVA_HOME
export JAVA_HOME="$JAVA_PATH"

# Add JAVA_HOME/bin to PATH
export PATH="$JAVA_HOME/bin:$PATH"

# Print confirmation
echo "JAVA_HOME set to $JAVA_HOME"
echo "PATH updated to include $JAVA_HOME/bin"
