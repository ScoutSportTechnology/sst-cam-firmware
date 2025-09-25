sudo apt-get update
sudo add-apt-repository -y universe
sudo apt-get update
sudo apt-get install -y \
  python3-gi \
  python3-gst-1.0 \
  gir1.2-gstreamer-1.0 \
  gstreamer1.0-tools \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-plugins-ugly \
  gstreamer1.0-libav
sudo apt-get install -y \
  libgstrtspserver-1.0-0 \
  gir1.2-gst-rtsp-server-1.0