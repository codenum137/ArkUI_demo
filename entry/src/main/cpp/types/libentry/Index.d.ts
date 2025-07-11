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

export const startVideoStream: (url: string, surfaceId: bigint) => VideoStreamResult;
export const stopVideoStream: (url: string) => boolean;
export const getStreamStatus: (url: string) => StreamStatus;
export const getFrameStats: (url: string) => FrameStats;
export const updateVideoSurfaceSize: (surfaceId: bigint, width: number, height: number) => boolean;
export const renderSingleTestFrame: (surfaceId: bigint, r?: number, g?: number, b?: number, a?: number) => boolean;

export const setSurfaceId: (id: bigint) => any;
export const changeSurface: (id: bigint, w: number, h: number) => any;
export const getXComponentStatus: (id: bigint) => XComponentContextStatus;
export const destroySurface: (id: bigint) => any;
