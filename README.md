# Event_Visualization

## Setup docker
```bash
## build docker
docker build -t event_viewer .

## setting GUI
sudo prime-select nvidia

## run docker
docker run -it --rm   --gpus all   --privileged   --ipc=host   -e "NVIDIA_VISIBLE_DEVICES=all"   -e "NVIDIA_DRIVER_CAPABILITIES=all"   -e "DISPLAY=$DISPLAY"   -v "/tmp/.X11-unix:/tmp/.X11-unix"   -v "/dev/dri:/dev/dri"   -v /path/to/local/dataset/:/workspace/   event_viewer   bash
```

## run
```bash
cd build
cmake ..
make

## dsec データセットの場合
## RGBは指定がなくてもよい。
cd ..
./build/event_viewer_3d /workspace/dataset/dsec/train/interlaken_00_c/events/left/events_2x.h5 /workspace/dataset/dsec/train/interlaken_00_c/images/               

```

↑↓　空間の長さを変更
←→　再生速度の変更
,.　表示する時間幅の変更
[]　RGB画像の透明度の変更
