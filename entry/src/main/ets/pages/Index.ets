import { hilog } from '@kit.PerformanceAnalysisKit';
import videoStreamNapi from 'libentry.so';

const DOMAIN = 0x0000;

// 定义接口
interface StreamInfo {
  url: string;
  isStreaming: boolean;
  info: string;
}

@Entry
@Component
struct Index {
  @State streamUrl: string = 'rtsp://77.110.228.219/axis-media/media.amp';
  @State isStreaming: boolean = false;
  @State streamInfo: string = 'No stream';
  @State logs: string[] = [];
  @State frameCount: number = 0;
  @State frameRate: number = 0;

  addLog(message: string): void {
    const timestamp = new Date().toLocaleTimeString();
    this.logs.unshift(`[${timestamp}] ${message}`);
    if (this.logs.length > 10) {
      this.logs = this.logs.slice(0, 10);
    }
    hilog.info(DOMAIN, 'VideoStream', message);
  }

  startStream(): void {
    if (!this.streamUrl.trim()) {
      this.addLog('请输入流地址');
      return;
    }

    this.addLog(`准备开始播放流: ${this.streamUrl}`);

    try {
      this.addLog('调用NAPI startVideoStream...');
      const result = videoStreamNapi.startVideoStream(this.streamUrl);
      this.addLog(`NAPI调用结果: success=${result.success}, url=${result.url}`);

      if (result.success) {
        this.isStreaming = true;
        this.addLog(`流启动成功: ${this.streamUrl}`);
        this.checkStreamStatus();
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
      this.isStreaming = status.isStreaming;
      this.streamInfo = status.info;

      // 获取真实的帧统计信息
      if (this.isStreaming) {
        const frameStats = videoStreamNapi.getFrameStats(this.streamUrl);
        this.frameCount = frameStats.frameCount;
        this.frameRate = frameStats.frameRate;

        // 如果还在播放，3秒后再次检查
        setTimeout((): void => {
          this.checkStreamStatus();
        }, 3000);
      }
    } catch (error) {
      this.addLog(`检查流状态异常: ${error}`);
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
              .backgroundColor(this.isStreaming ? '#9ca3af' : '#10b981')
              .fontColor(Color.White)
              .enabled(!this.isStreaming)
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
            Text('视频显示')
              .fontSize(16)
              .fontWeight(FontWeight.Medium)
              .alignSelf(ItemAlign.Start)
              .margin({ bottom: 10 })

            Stack() {
              // 视频渲染组件
              // XComponent({
              //   id: 'videoCanvas',
              //   type: 'surface',
              //   libraryname: 'entry'
              // })
              //   .width('100%')
              //   .height(200)
              //   .backgroundColor('#000000')
              //   .borderRadius(8)
              //   .onLoad((event) => {
              //     this.addLog('视频画布加载完成');
              //   })
              //   .onDestroy(() => {
              //     this.addLog('视频画布销毁');
              //   })

              // 播放状态指示
              Column() {
                if (this.isStreaming) {
                  Text('🔴 LIVE')
                    .fontSize(16)
                    .fontColor('#ef4444')
                    .fontWeight(FontWeight.Bold)

                  Text(`帧数: ${this.frameCount}`)
                    .fontSize(12)
                    .fontColor('#d1d5db')
                    .margin({ top: 3 })

                  Text(`帧率: ${this.frameRate.toFixed(1)} fps`)
                    .fontSize(12)
                    .fontColor('#d1d5db')
                    .margin({ top: 2 })
                } else {
                  Text('⏸️ 未播放')
                    .fontSize(16)
                    .fontColor('#9ca3af')
                }

                Text(this.streamInfo)
                  .fontSize(12)
                  .fontColor('#d1d5db')
                  .margin({ top: 5 })
                  .maxLines(2)
                  .textOverflow({ overflow: TextOverflow.Ellipsis })
              }
              .alignItems(HorizontalAlign.Center)
            }
            .alignContent(Alignment.Center)
          }
          .width('100%')
          .padding(15)
          .backgroundColor(Color.White)
          .borderRadius(10)
          .margin({ bottom: 15 })

          // 日志区域
          Column() {
            Text('运行日志')
              .fontSize(16)
              .fontWeight(FontWeight.Medium)
              .alignSelf(ItemAlign.Start)
              .margin({ bottom: 10 })

            Scroll() {
              Column() {
                if (this.logs.length > 0) {
                  ForEach(this.logs, (log: string) => {
                    Text(log)
                      .fontSize(12)
                      .fontColor('#374151')
                      .alignSelf(ItemAlign.Start)
                      .width('100%')
                      .padding({ top: 2, bottom: 2 })
                  })
                } else {
                  Text('暂无日志')
                    .fontSize(12)
                    .fontColor('#9ca3af')
                    .textAlign(TextAlign.Center)
                    .width('100%')
                    .padding(10)
                }
              }
              .width('100%')
            }
            .width('100%')
            .height(150)
            .backgroundColor('#f9fafb')
            .borderRadius(6)
            .padding(10)
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
