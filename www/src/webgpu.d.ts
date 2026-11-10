// Minimal WebGPU ambient declarations so `tsc` works even before `npm install`
type GPUTextureFormat = 'bgra8unorm' | 'rgba8unorm' | 'rgba16float' | string;
type GPUCanvasAlphaMode = 'opaque' | 'premultiplied';
type GPULoadOp = 'load' | 'clear';
type GPUStoreOp = 'store' | 'discard';

interface GPUColor {
  r: number;
  g: number;
  b: number;
  a: number;
}

interface GPUCanvasConfiguration {
  device: GPUDevice;
  format: GPUTextureFormat;
  alphaMode?: GPUCanvasAlphaMode;
}

interface GPUTextureView {}

interface GPUTexture {
  createView(): GPUTextureView;
}

interface GPUCanvasContext {
  configure(configuration: GPUCanvasConfiguration): void;
  unconfigure(): void;
  getCurrentTexture(): GPUTexture;
}

interface GPURenderPassColorAttachment {
  view: GPUTextureView;
  clearValue?: GPUColor;
  loadOp: GPULoadOp;
  storeOp: GPUStoreOp;
}

interface GPURenderPassDescriptor {
  colorAttachments: GPURenderPassColorAttachment[];
}

interface GPURenderPassEncoder {
  end(): void;
}

interface GPUCommandBuffer {}

interface GPUCommandEncoder {
  beginRenderPass(descriptor: GPURenderPassDescriptor): GPURenderPassEncoder;
  finish(): GPUCommandBuffer;
}

interface GPUQueue {
  submit(commandBuffers: GPUCommandBuffer[]): void;
}

interface GPUDeviceLostInfo {
  readonly reason: string;
  readonly message: string;
}

interface GPUDevice {
  readonly queue: GPUQueue;
  readonly lost: Promise<GPUDeviceLostInfo>;
  createCommandEncoder(): GPUCommandEncoder;
  destroy(): void;
}

interface GPUAdapter {
  requestDevice(): Promise<GPUDevice>;
}

interface GPU {
  requestAdapter(): Promise<GPUAdapter | null>;
  getPreferredCanvasFormat(): GPUTextureFormat;
}

interface Navigator {
  readonly gpu?: GPU;
}

interface HTMLCanvasElement {
  getContext(contextId: 'webgpu'): GPUCanvasContext | null;
}
