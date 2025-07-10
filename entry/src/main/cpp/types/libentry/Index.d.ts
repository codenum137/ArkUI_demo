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

type XComponentContextStatus = {
  hasDraw: boolean,
  hasChangeColor: boolean,
};

export const startVideoStream: (url: string) => VideoStreamResult;
export const stopVideoStream: (url: string) => boolean;
export const getStreamStatus: (url: string) => StreamStatus;
export const getFrameStats: (url: string) => FrameStats;

export const SetSurfaceId: (id: BigInt) => any;
export const ChangeSurface: (id: BigInt, w: number, h: number) =>any;
export const DrawPattern: (id: BigInt) => any;
export const GetXComponentStatus: (id: BigInt) => XComponentContextStatus
export const DestroySurface: (id: BigInt) => any;
