# IF JAVA_HOME NOT PRESENT
Steps:
    1. Find JAVA path
    2. Add to sethome.sh
    3. Run `source sethome.sh`

# Compile JAVA code
`javac test-programs/*filename*.java`

# COMPILE AGENT
gcc -shared -fPIC agent.c -o libagent.so \
  -I$JAVA_HOME/include \
  -I$JAVA_HOME/include/linux
