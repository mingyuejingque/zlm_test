# zlm_test
### ZLMediaKit 测试

### 测试ZLMediaKit的 mk_api 提供的推流功能，写了一个小例子，实现了：  
  * 用ffmpeg来读文件来塞给 mk_api, 
  * mk_api 推 ps 流给 ZLM2， 
  * 在 ZLM2 上可以用webrtc， rtmp，rtsp， ws 等等方式来播放该流。
  
### 注意：
  * 代码中并未对音视频同步做保证
  * 音频默认为 g711a, 8k, ch1.
  * 视频默认为 h264
  * Makefile 里依赖的路径 和 一些so库自行调整。
  
  
