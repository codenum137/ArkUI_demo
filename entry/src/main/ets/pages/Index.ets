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

  onSurfaceCreated(surfaceId: string): void {
    console.log(`onSurfaceCreated surfaceId: ${surfaceId}`);
    videoStreamNapi.setSurfaceId(BigInt(surfaceId));
    if (this.parentComponent) {
      this.parentComponent.setSurfaceId(surfaceId);
    }
  }
  onSurfaceChanged(surfaceId: string, rect: SurfaceRect): void {
    console.log(`onSurfaceChanged surfaceId: ${surfaceId}, rect: ${JSON.stringify(rect)}}`);
    videoStreamNapi.changeSurface(BigInt(surfaceId), rect.surfaceWidth, rect.surfaceHeight);
  }
  onSurfaceDestroyed(surfaceId: string): void {
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
  // @State streamUrl: string = 'rtsp://77.110.228.219/axis-media/media.amp';
  @State streamUrl: string ='rtsp://10.129.177.159:8554/mystream';
  @State isStreaming: boolean = false;
  @State streamInfo: string = 'No stream';
  @State frameCount: number = 0;
  @State frameRate: number = 0;
  @State surfaceId: string = '';
  xComponentController: MyXComponentController = new MyXComponentController();

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
        // 延迟1秒后开始检查状态，让C++端有时间建立连接
        setTimeout(() => {
          this.checkStreamStatus();
        }, 1000);
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
      } else {
        this.addLog(`停止流失败: ${this.streamUrl}`);
      }
    } catch (error) {
      this.addLog(`停止流异常: ${error}`);
    }
  }

  checkStreamStatus(): void {
    if (!this.streamUrl.trim()) return;

    try {
      const status = videoStreamNapi.getStreamStatus(this.streamUrl);
      this.addLog(`状态检查: isStreaming=${status.isStreaming}, info=${status.info}`);
      
      this.isStreaming = status.isStreaming;
      this.streamInfo = status.info;

      // 获取真实的帧统计信息
      if (this.isStreaming) {
        const frameStats = videoStreamNapi.getFrameStats(this.streamUrl);
        this.frameCount = frameStats.frameCount;
        this.frameRate = frameStats.frameRate;
        this.addLog(`帧统计: count=${this.frameCount}, rate=${this.frameRate.toFixed(1)}fps`);

        // 如果还在播放，3秒后再次检查
        setTimeout((): void => {
          this.checkStreamStatus();
        }, 3000);
      } else {
        this.addLog('流未在播放状态');
      }
    } catch (error) {
      this.addLog(`检查流状态异常: ${error}`);
    }
  }

  renderTestFrame(r: number, g: number, b: number, a: number): void {
    if (!this.surfaceId) {
      this.addLog('Surface未准备好，无法渲染测试帧');
      return;
    }

    this.addLog(`渲染测试帧: 颜色(${r}, ${g}, ${b}, ${a})`);

    try {
      const result = videoStreamNapi.renderSingleTestFrame(BigInt(this.surfaceId), r, g, b, a);
      this.addLog(`测试帧渲染结果: ${result ? '成功' : '失败'}`);
    } catch (error) {
      this.addLog(`渲染测试帧异常: ${error}`);
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

          // 测试按钮区域
          Column() {
            Text('渲染测试')
              .fontSize(16)
              .fontWeight(FontWeight.Medium)
              .alignSelf(ItemAlign.Start)
              .margin({ bottom: 10 })

            Row() {
              Button('蓝色测试帧')
                .width('30%')
                .height(35)
                .fontSize(14)
                .backgroundColor('#3b82f6')
                .fontColor(Color.White)
                .enabled(!!this.surfaceId)
                .onClick(() => this.renderTestFrame(0.2, 0.6, 0.9, 1.0))

              Button('红色测试帧')
                .width('30%')
                .height(35)
                .fontSize(14)
                .backgroundColor('#ef4444')
                .fontColor(Color.White)
                .enabled(!!this.surfaceId)
                .onClick(() => this.renderTestFrame(0.9, 0.2, 0.2, 1.0))

              Button('绿色测试帧')
                .width('30%')
                .height(35)
                .fontSize(14)
                .backgroundColor('#10b981')
                .fontColor(Color.White)
                .enabled(!!this.surfaceId)
                .onClick(() => this.renderTestFrame(0.2, 0.9, 0.2, 1.0))
            }
            .width('100%')
            .justifyContent(FlexAlign.SpaceBetween)
          }
          .width('100%')
          .padding(15)
          .backgroundColor(Color.White)
          .borderRadius(10)
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
            Row() {
              if (this.isStreaming) {
                Text('🔴 LIVE')
                  .fontSize(14)
                  .fontColor('#ef4444')
                  .fontWeight(FontWeight.Bold)

                Text(`帧数: ${this.frameCount}`)
                  .fontSize(12)
                  .fontColor('#6b7280')
                  .margin({ left: 15 })

                Text(`帧率: ${this.frameRate.toFixed(1)} fps`)
                  .fontSize(12)
                  .fontColor('#6b7280')
                  .margin({ left: 10 })
              } else {
                Text('⏸️ 未播放')
                  .fontSize(14)
                  .fontColor('#9ca3af')
              }
            }
            .width('100%')
            .justifyContent(FlexAlign.Start)
            .alignItems(VerticalAlign.Center)
            .margin({ top: 8 })

            // 流信息显示
            if (this.streamInfo && this.streamInfo !== 'No stream') {
              Text(this.streamInfo)
                .fontSize(11)
                .fontColor('#9ca3af')
                .margin({ top: 4 })
                .maxLines(2)
                .textOverflow({ overflow: TextOverflow.Ellipsis })
                .width('100%')
            }
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
