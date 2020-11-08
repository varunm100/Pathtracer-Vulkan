if [ "$1" = "-c" ]; then
  cmake -H. -G Ninja -BDebug -DCMAKE_EXPORT_COMPILE_COMMANDS=YES
  if [ $? -ne 0 ]; then
    echo "Configuring failed!"
    exit 1
  fi
  cp Debug/compile_commands.json .
fi
ninja -C Debug
if [ $? -ne 0 ]; then
  echo "Compilation failed!"
  exit 1
fi
./Debug/RaytracingTest