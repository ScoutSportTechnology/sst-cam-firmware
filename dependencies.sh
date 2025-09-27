sudo apt-get update
sudo add-apt-repository -y universe
sudo apt-get update
sudo apt-get install -y \
	build-essential \
	gir1.2-gst-rtsp-server-1.0 \
	gir1.2-gstreamer-1.0 \
	gobject-introspection \
	gstreamer1.0-libav \
	gstreamer1.0-plugins-bad \
	gstreamer1.0-plugins-base \
	gstreamer1.0-plugins-good \
	gstreamer1.0-plugins-ugly \
	gstreamer1.0-tools \
	libcairo2-dev \
	libffi-dev \
	libgirepository1.0-dev \
	libgstrtspserver-1.0-0 \
	libgstreamer1.0-dev \
	pkg-config \
	python3-dev \
	python3-gi \
	python3-gi-cairo \
	python3-gst-1.0
