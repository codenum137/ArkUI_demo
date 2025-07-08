export interface VideoStreamResult {
  success: boolean;
  url: string;
}

export interface StreamStatus {
  isStreaming: boolean;
  info: string;
}

export interface ActiveStream {
  url: string;
  info: string;
}

export interface VideoFrame {
  width: number;
  height: number;
  pts: number;
  data: ArrayBuffer;
}

export interface FrameStats {
  frameCount: number;
  frameRate: number;
}

export const startVideoStream: (url: string) => VideoStreamResult;
export const stopVideoStream: (url: string) => boolean;
export const getStreamStatus: (url: string) => StreamStatus;
export const getFrameStats: (url: string) => FrameStats;