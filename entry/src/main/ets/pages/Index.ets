import { hilog } from '@kit.PerformanceAnalysisKit';
import videoStreamNapi from 'libentry.so';

const DOMAIN = 0x0000;

// 定义接口
interface StreamInfo {
  url: string;
  isStreaming: boolean;
  info: string;
}

// 视频流状态机枚举
enum StreamState {
  IDLE = 'idle',           // 空闲状态
  STARTING = 'starting',   // 启动中
  RUNNING = 'running',     // 运行中
  STOPPING = 'stopping',   // 结束中
  ERROR = 'error'          // 错误状态
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
  // @State streamUrl: string = 'rtsp://10.129.177.159:8554/mystream'; //hey
  @State streamUrl: string = 'rtsp://10.29.237.55:8554/live';
  // @State streamUrl: string ='rtsp://localhost:8554/live';
  @State isStreaming: boolean = false;
  @State streamInfo: string = 'No stream';
  @State frameCount: number = 0;
  @State frameRate: number = 0;
  @State surfaceId: string = '';
  
  // 状态机相关
  @State currentState: StreamState = StreamState.IDLE;
  @State stateMessage: string = '准备就绪';
  
  xComponentController: MyXComponentController = new MyXComponentController();
  private statusTimer: number = -1;
  private stateTransitionTimer: number = -1;

  aboutToAppear(): void {
    this.xComponentController.setParent(this);
    this.transitionToState(StreamState.IDLE, '等待用户操作');
  }

  aboutToDisappear(): void {
    // 组件销毁时强制清理所有资源
    this.forceCleanup();
  }

  setSurfaceId(id: string): void {
    this.surfaceId = id;
    console.log(`Surface ID updated: ${id}`);
  }

  addLog(message: string): void {
    const stateInfo = `[${this.currentState}] ${message}`;
    console.log(`[VideoStream] ${stateInfo}`);
    hilog.info(DOMAIN, 'VideoStream', stateInfo);
  }

  // 状态机核心方法
  transitionToState(newState: StreamState, message?: string): void {
    const oldState = this.currentState;
    this.currentState = newState;
    this.stateMessage = message || this.getStateDescription(newState);
    
    this.addLog(`状态转换: ${oldState} -> ${newState} (${this.stateMessage})`);
    
    // 根据新状态执行相应动作
    this.executeStateAction(newState);
  }

  getStateDescription(state: StreamState): string {
    switch (state) {
      case StreamState.IDLE: return '空闲状态';
      case StreamState.STARTING: return '正在启动...';
      case StreamState.RUNNING: return '播放中';
      case StreamState.STOPPING: return '正在结束...';
      case StreamState.ERROR: return '发生错误';
      default: return '未知状态';
    }
  }

  executeStateAction(state: StreamState): void {
    // 清理之前的状态转换定时器
    if (this.stateTransitionTimer !== -1) {
      clearTimeout(this.stateTransitionTimer);
      this.stateTransitionTimer = -1;
    }

    switch (state) {
      case StreamState.IDLE:
        this.isStreaming = false;
        this.frameCount = 0;
        this.frameRate = 0;
        this.streamInfo = 'No stream';
        this.stopStatusPolling();
        break;
        
      case StreamState.STARTING:
        this.performStart();
        break;
        
      case StreamState.RUNNING:
        this.isStreaming = true;
        this.startStatusPolling();
        break;
        
      case StreamState.STOPPING:
        this.performStop();
        break;
        
      case StreamState.ERROR:
        this.isStreaming = false;
        this.stopStatusPolling();
        // 错误状态5秒后自动回到IDLE
        this.stateTransitionTimer = setTimeout(() => {
          this.transitionToState(StreamState.IDLE, '错误恢复');
        }, 5000);
        break;
    }
  }

  // 检查状态转换是否允许
  canTransitionTo(targetState: StreamState): boolean {
    const transitions: Record<StreamState, StreamState[]> = {
      [StreamState.IDLE]: [StreamState.STARTING],
      [StreamState.STARTING]: [StreamState.RUNNING, StreamState.ERROR, StreamState.IDLE],
      [StreamState.RUNNING]: [StreamState.STOPPING, StreamState.ERROR],
      [StreamState.STOPPING]: [StreamState.IDLE, StreamState.ERROR],
      [StreamState.ERROR]: [StreamState.IDLE]
    };
    
    return transitions[this.currentState]?.includes(targetState) || false;
  }

  startStream(): void {
    if (!this.canTransitionTo(StreamState.STARTING)) {
      this.addLog(`当前状态 ${this.currentState} 不允许启动流`);
      return;
    }

    if (!this.streamUrl.trim()) {
      this.addLog('请输入流地址');
      return;
    }

    if (!this.surfaceId) {
      this.addLog('Surface未准备好，请稍后重试');
      return;
    }

    this.transitionToState(StreamState.STARTING, `准备启动流: ${this.streamUrl}`);
  }

  performStart(): void {
    try {
      this.addLog('调用NAPI startVideoStream...');
      const result = videoStreamNapi.startVideoStream(this.streamUrl, BigInt(this.surfaceId));
      this.addLog(`NAPI调用结果: success=${result.success}, url=${result.url}`);

      if (result.success) {
        // 启动成功，但需要等待实际连接建立
        this.stateTransitionTimer = setTimeout(() => {
          this.transitionToState(StreamState.RUNNING, '流启动成功');
        }, 5000);
      } else {
        this.transitionToState(StreamState.ERROR, '启动流失败');
      }
    } catch (error) {
      this.addLog(`启动流异常: ${error}`);
      this.transitionToState(StreamState.ERROR, `启动异常: ${error}`);
    }
  }

  stopStream(): void {
    if (!this.canTransitionTo(StreamState.STOPPING)) {
      this.addLog(`当前状态 ${this.currentState} 不允许结束流`);
      return;
    }

    if (!this.streamUrl.trim()) {
      this.addLog('请输入流地址');
      return;
    }

    this.transitionToState(StreamState.STOPPING, `准备结束流: ${this.streamUrl}`);
  }

  performStop(): void {
    try {
      const result = videoStreamNapi.stopVideoStream(this.streamUrl);
      if (result) {
        this.addLog(`结束播放流成功: ${this.streamUrl}`);
        // 给C++层充分时间清理资源
        this.stateTransitionTimer = setTimeout(() => {
          this.transitionToState(StreamState.IDLE, '流已结束');
        }, 2000);
      } else {
        this.transitionToState(StreamState.ERROR, '结束流失败');
      }
    } catch (error) {
      this.addLog(`结束流异常: ${error}`);
      this.transitionToState(StreamState.ERROR, `结束异常: ${error}`);
    }
  }

  forceCleanup(): void {
    this.addLog('执行强制清理...');
    this.stopStatusPolling();
    
    if (this.stateTransitionTimer !== -1) {
      clearTimeout(this.stateTransitionTimer);
      this.stateTransitionTimer = -1;
    }

    try {
      if (this.streamUrl.trim()) {
        // 网络资源对称清理：确保C++层完成网络资源清理
        this.addLog('正在清理网络资源...');
        const result = videoStreamNapi.stopVideoStream(this.streamUrl);
        this.addLog(`网络资源清理结果: ${result}`);
        
        // 给C++层更多时间进行网络清理
        setTimeout(() => {
          this.addLog('网络资源清理完成');
        }, 1000);
      }
    } catch (error) {
      this.addLog(`强制清理异常: ${error}`);
    }
    
    this.transitionToState(StreamState.IDLE, '强制清理完成');
  }

  // UI状态控制方法
  canStartStream(): boolean {
    return this.currentState === StreamState.IDLE && !!this.surfaceId;
  }

  canStopStream(): boolean {
    return this.currentState === StreamState.RUNNING || this.currentState === StreamState.STARTING;
  }

  getStartButtonText(): string {
    switch (this.currentState) {
      case StreamState.STARTING: return '启动中...';
      case StreamState.RUNNING: return '播放中';
      case StreamState.STOPPING: return '结束中...';
      default: return '开始播放';
    }
  }

  getStopButtonText(): string {
    switch (this.currentState) {
      case StreamState.STOPPING: return '结束中...';
      case StreamState.STARTING: return '取消启动';
      default: return '结束播放';
    }
  }

  getStartButtonColor(): string {
    if (!this.surfaceId) return '#f59e0b'; // 橙色：Surface未准备
    if (this.currentState === StreamState.IDLE) return '#10b981'; // 绿色：可以启动
    return '#9ca3af'; // 灰色：不可操作
  }

  getStopButtonColor(): string {
    if (this.currentState === StreamState.RUNNING || this.currentState === StreamState.STARTING) {
      return '#ef4444'; // 红色：可以结束
    }
    return '#9ca3af'; // 灰色：不可操作
  }

  getStateColor(): string {
    switch (this.currentState) {
      case StreamState.IDLE: return '#6b7280';
      case StreamState.STARTING: return '#f59e0b';
      case StreamState.RUNNING: return '#10b981';
      case StreamState.STOPPING: return '#f59e0b';
      case StreamState.ERROR: return '#ef4444';
      default: return '#6b7280';
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
      this.stopStatusPolling();
      return;
    }

    // 只在RUNNING状态或STARTING状态检查
    if (this.currentState !== StreamState.RUNNING && this.currentState !== StreamState.STARTING) {
      return;
    }

    try {
      const status = videoStreamNapi.getStreamStatus(this.streamUrl);
      this.addLog(`状态检查: isStreaming=${status.isStreaming}, info=${status.info}`);

      // 根据实际流状态和当前状态机状态进行处理
      if (status.isStreaming) {
        // 流正在运行
        if (this.currentState === StreamState.STARTING) {
          // 从启动状态转为运行状态
          this.transitionToState(StreamState.RUNNING, '流连接已建立');
        }
        
        // 更新统计信息
        this.streamInfo = status.info;
        const frameStats = videoStreamNapi.getFrameStats(this.streamUrl);
        this.frameCount = frameStats.frameCount;
        this.frameRate = frameStats.frameRate;
        this.addLog(`帧统计: count=${this.frameCount}, rate=${this.frameRate.toFixed(1)}fps`);
      } else {
        // 流未运行
        if (this.currentState === StreamState.RUNNING) {
          // 运行中但检测到流断开
          this.transitionToState(StreamState.ERROR, '流连接已断开');
        } else if (this.currentState === StreamState.STARTING) {
          // 仍在连接中，更新信息但不改变状态
          this.streamInfo = status.info || '正在建立连接...';
        }
      }
    } catch (error) {
      this.addLog(`检查流状态异常: ${error}`);
      if (this.currentState === StreamState.RUNNING || this.currentState === StreamState.STARTING) {
        this.transitionToState(StreamState.ERROR, `状态检查异常: ${error}`);
      }
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
            Button(this.getStartButtonText())
              .width('48%')
              .height(40)
              .fontSize(16)
              .backgroundColor(this.getStartButtonColor())
              .fontColor(Color.White)
              .enabled(this.canStartStream())
              .onClick(() => this.startStream())

            Button(this.getStopButtonText())
              .width('48%')
              .height(40)
              .fontSize(16)
              .backgroundColor(this.getStopButtonColor())
              .fontColor(Color.White)
              .enabled(this.canStopStream())
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

              Column() {
                Text(this.surfaceId ? `Surface: ${this.surfaceId.substring(0, 8)}...` : 'Surface: 未初始化')
                  .fontSize(12)
                  .fontColor(this.surfaceId ? '#10b981' : '#ef4444')
                
                Text(`状态: ${this.stateMessage}`)
                  .fontSize(11)
                  .fontColor(this.getStateColor())
                  .margin({ top: 2 })
              }
              .alignItems(HorizontalAlign.End)
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
