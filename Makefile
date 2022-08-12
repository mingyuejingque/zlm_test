all: *.cpp
	g++ -o test *.cpp \
		-L/home/test/works/ZLMediaKit/release/linux/Debug \
		-L/home/test/projects/cpp/ffmpeg/install_dir/lib \
		-I/home/test/projects/cpp/ffmpeg/install_dir/include \
		-I/home/test/works/ZLMediaKit/api/include \
		-lpthread \
		-lmk_api \
		-lavformat -lswscale -lavcodec -lavdevice  -lswresample -lavutil -lavfilter \
		-lz -lX11 -lxcb -lva \
		-lxcb -lxcb-shm -lxcb-shape -lxcb-xfixes -lva -lz -lva-drm -lva-x11 -lvdpau \
		-lXau -lXdmcp -ldl -lrt \
		-lm \

