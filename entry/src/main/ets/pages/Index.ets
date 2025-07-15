import { hilog } from '@kit.PerformanceAnalysisKit';
import videoStreamNapi from 'libentry.so';

const DOMAIN = 0x0000;

// 定义接口
interface StreamInfo {
  url: string;
  isStreaming: boolean;
  info: string;
}

class MyXComponentController extends XComponentController{
  private parentComponent: Index | null = null;

  setParent(parent: Index) {
    this.parentComponent = parent;
  }

  onSurfaceCreated = (surfaceId: string): void => {
    console.log(`onSurfaceCreated surfaceId: ${surfaceId}`);
    videoStreamNapi.setSurfaceId(BigInt(surfaceId));
    if (this.parentComponent) {
      this.parentComponent.setSurfaceId(surfaceId);
    }
  }
  onSurfaceChanged = (surfaceId: string, rect: SurfaceRect): void => {
    console.log(`onSurfaceChanged surfaceId: ${surfaceId}, rect: ${JSON.stringify(rect)}}`);
    videoStreamNapi.changeSurface(BigInt(surfaceId), rect.surfaceWidth, rect.surfaceHeight);
  }
  onSurfaceDestroyed = (surfaceId: string): void => {
    console.log(`onSurfaceDestroyed surfaceId: ${surfaceId}`);
    videoStreamNapi.destroySurface(BigInt(surfaceId));
    if (this.parentComponent) {
      this.parentComponent.setSurfaceId('');
    }
  }
}

@Entry
@Component
struct Index {
  @State streamUrl: string = 'rtsp://10.29.237.55:8554/stream';
  // @State streamUrl: string ='rtsp://77.110.228.219/axis-media/media.amp';
  @State isStreaming: boolean = false;
  @State streamInfo: string = 'No stream';
  @State frameCount: number = 0;
  @State frameRate: number = 0;
  @State surfaceId: string = '';
  xComponentController: MyXComponentController = new MyXComponentController();
  private statusTimer: number = -1;

  aboutToAppear(): void {
    this.xComponentController.setParent(this);
  }

  setSurfaceId(id: string): void {
    this.surfaceId = id;
    console.log(`Surface ID updated: ${id}`);
  }

  addLog(message: string): void {
    // 简化为只输出console日志
    console.log(`[VideoStream] ${message}`);
    hilog.info(DOMAIN, 'VideoStream', message);
  }

  startStream(): void {
    if (!this.streamUrl.trim()) {
      this.addLog('请输入流地址');
      return;
    }

    if (!this.surfaceId) {
      this.addLog('Surface未准备好，请稍后重试');
      return;
    }

    this.addLog(`准备开始播放流: ${this.streamUrl}`);

    try {
      this.addLog('调用NAPI startVideoStream...');
      const result = videoStreamNapi.startVideoStream(this.streamUrl, BigInt(this.surfaceId));
      this.addLog(`NAPI调用结果: success=${result.success}, url=${result.url}`);

      if (result.success) {
        this.addLog(`流启动成功: ${this.streamUrl}`);
        this.startStatusPolling();
      } else {
        this.addLog(`启动流失败: ${this.streamUrl}`);
      }
    } catch (error) {
      this.addLog(`启动流异常: ${error}`);
    }
  }

  stopStream(): void {
    if (!this.streamUrl.trim()) {
      this.addLog('请输入流地址');
      return;
    }

    try {
      const result = videoStreamNapi.stopVideoStream(this.streamUrl);
      if (result) {
        this.isStreaming = false;
        this.streamInfo = 'Stream stopped';
        this.frameCount = 0;
        this.frameRate = 0;
        this.addLog(`停止播放流: ${this.streamUrl}`);
        this.stopStatusPolling();
      } else {
        this.addLog(`停止流失败: ${this.streamUrl}`);
      }
    } catch (error) {
      this.addLog(`停止流异常: ${error}`);
    }
  }

  startStatusPolling(): void {
    this.stopStatusPolling(); // 先清除旧的计时器，以防万一
    // 延迟1秒后开始，让C++端有时间建立连接
    setTimeout(() => {
      this.checkStreamStatus(); // 立即执行一次
      // 然后设置定时器，以3秒为间隔重复执行
      this.statusTimer = setInterval(() => {
        this.checkStreamStatus();
      }, 3000);
    }, 1000);
  }

  stopStatusPolling(): void {
    if (this.statusTimer !== -1) {
      clearInterval(this.statusTimer);
      this.statusTimer = -1;
      this.addLog('状态检查计时器已停止');
    }
  }

  checkStreamStatus(): void {
    if (!this.streamUrl.trim()) {
      this.stopStatusPolling(); // 如果URL为空，停止轮询
      return;
    }

    try {
      const status = videoStreamNapi.getStreamStatus(this.streamUrl);
      this.addLog(`状态检查: isStreaming=${status.isStreaming}, info=${status.info}`);

      // Case 1: Stream is currently running.
      if (status.isStreaming) {
        this.isStreaming = true; // Ensure UI state is true
        this.streamInfo = status.info;

        const frameStats = videoStreamNapi.getFrameStats(this.streamUrl);
        this.frameCount = frameStats.frameCount;
        this.frameRate = frameStats.frameRate;
        this.addLog(`帧统计: count=${this.frameCount}, rate=${this.frameRate.toFixed(1)}fps`);
      } 
      // Case 2: Stream is NOT currently running.
      else {
        // Sub-case 2a: It WAS running before, but has now stopped.
        if (this.isStreaming) {
          this.addLog('流连接已断开，停止轮询。');
          this.isStreaming = false; // Update UI state
          this.streamInfo = 'Stream stopped';
          this.frameCount = 0;
          this.frameRate = 0;
          this.stopStatusPolling(); // Stop the timer
        } 
        // Sub-case 2b: It was not running before, and is still not running.
        else {
          // This is the "connecting" phase. Do nothing and let the timer try again.
          this.addLog('正在等待流连接建立...');
          this.streamInfo = status.info; // Update info text even while connecting
        }
      }
    } catch (error) {
      this.addLog(`检查流状态异常: ${error}`);
      this.stopStatusPolling(); // Stop polling on any error
    }
  }

  build() {
    Column() {
      // 标题
      Row() {
        Text('实时视频直播应用')
          .fontSize(24)
          .fontWeight(FontWeight.Bold)
          .fontColor(Color.White)
      }
      .width('100%')
      .height(60)
      .backgroundColor('#1f2937')
      .justifyContent(FlexAlign.Center)
      .alignItems(VerticalAlign.Center)

      Scroll() {
        Column() {
          // 流地址输入区域
          Column() {
            Text('流地址设置')
              .fontSize(18)
              .fontWeight(FontWeight.Medium)
              .alignSelf(ItemAlign.Start)
              .margin({ bottom: 10 })

            TextInput({
              placeholder: '请输入RTSP/RTP/RTMP流地址',
              text: this.streamUrl
            })
              .width('100%')
              .height(40)
              .fontSize(14)
              .backgroundColor('#f8f9fa')
              .borderRadius(8)
              .border({ width: 1, color: '#d1d5db' })
              .padding({ left: 12, right: 12 })
              .focusable(true)
              .onChange((value: string) => {
                this.streamUrl = value;
              })
              .onFocus(() => {
                // 获得焦点时选中全部文本（如果支持的话）
              })
          }
          .width('100%')
          .padding(15)
          .backgroundColor(Color.White)
          .borderRadius(10)
          .margin({ bottom: 15 })

          // 控制按钮
          Row() {
            Button('开始播放')
              .width('48%')
              .height(40)
              .fontSize(16)
              .backgroundColor(this.isStreaming ? '#9ca3af' : (!this.surfaceId ? '#f59e0b' : '#10b981'))
              .fontColor(Color.White)
              .enabled(!this.isStreaming && !!this.surfaceId)
              .onClick(() => this.startStream())

            Button('停止播放')
              .width('48%')
              .height(40)
              .fontSize(16)
              .backgroundColor(this.isStreaming ? '#ef4444' : '#9ca3af')
              .fontColor(Color.White)
              .enabled(this.isStreaming)
              .onClick(() => this.stopStream())
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceBetween)
          .padding({ left: 15, right: 15 })
          .margin({ bottom: 15 })

          // 视频显示区域
          Column() {
            Row() {
              Text('视频显示')
                .fontSize(16)
                .fontWeight(FontWeight.Medium)
                .layoutWeight(1)

              Text(this.surfaceId ? `Surface: ${this.surfaceId.substring(0, 8)}...` : 'Surface: 未初始化')
                .fontSize(12)
                .fontColor(this.surfaceId ? '#10b981' : '#ef4444')
            }
            .width('100%')
            .margin({ bottom: 10 })

            // 视频渲染组件XComponent
            XComponent({
              type: XComponentType.SURFACE,
              controller: this.xComponentController
            })
              .width('100%')
              .height(200)
              .backgroundColor('#000000')
              .borderRadius(8)

            // 播放状态指示 - 移到视频下方
            // Row() {
            //   if (this.isStreaming) {
            //     Text('🔴 LIVE')
            //       .fontSize(14)
            //       .fontColor('#ef4444')
            //       .fontWeight(FontWeight.Bold)
            //
            //     Text(`帧数: ${this.frameCount}`)
            //       .fontSize(12)
            //       .fontColor('#6b7280')
            //       .margin({ left: 15 })
            //
            //     Text(`帧率: ${this.frameRate.toFixed(1)} fps`)
            //       .fontSize(12)
            //       .fontColor('#6b7280')
            //       .margin({ left: 10 })
            //   } else {
            //     Text('⏸️ 未播放')
            //       .fontSize(14)
            //       .fontColor('#9ca3af')
            //   }
            // }
            // .width('100%')
            // .justifyContent(FlexAlign.Start)
            // .alignItems(VerticalAlign.Center)
            // .margin({ top: 8 })

            // 流信息显示
            // if (this.streamInfo && this.streamInfo !== 'No stream') {
            //   Text(this.streamInfo)
            //     .fontSize(11)
            //     .fontColor('#9ca3af')
            //     .margin({ top: 4 })
            //     .maxLines(2)
            //     .textOverflow({ overflow: TextOverflow.Ellipsis })
            //     .width('100%')
            // }
          }
          .width('100%')
          .padding(15)
          .backgroundColor(Color.White)
          .borderRadius(10)
          .margin({ bottom: 15 })

        }
        .width('100%')
        .padding(15)
      }
      .width('100%')
      .layoutWeight(1)
      .backgroundColor('#f3f4f6')
    }
    .width('100%')
    .height('100%')
  }
}
