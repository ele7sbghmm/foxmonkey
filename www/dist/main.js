const FENCE_MAGIC = 0x45434e46; // 'FNCE' in little-endian

function setStatus(id, text, ok) {
    const el = document.getElementById(id);
    if (el) {
        el.textContent = text;
        el.className = ok ? 'status-ok' : 'status-err';
    }
}

function setText(id, text) {
    const el = document.getElementById(id);
    if (el) {
        el.textContent = text;
    }
}

const FENCE_WGSL = /* wgsl */ `
struct ViewUniforms {
  center: vec2<f32>,
  scale: vec2<f32>,
};

@group(0) @binding(0) var<uniform> view: ViewUniforms;

struct VertexOut {
  @builtin(position) position: vec4<f32>,
};

@vertex
fn vs_main(@location(0) worldXZ: vec2<f32>) -> VertexOut {
  var out: VertexOut;
  let ndc = (worldXZ - view.center) * view.scale;
  out.position = vec4<f32>(ndc.x, ndc.y, 0.0, 1.0);
  return out;
}

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  return vec4<f32>(1.0, 0.15, 0.15, 1.0);
}
`;

export async function initWebGPU(canvas) {
    if (!navigator.gpu) {
        throw new Error('WebGPU is not supported in this browser.');
    }
    const adapter = await navigator.gpu.requestAdapter();
    if (!adapter) {
        throw new Error('Failed to request WebGPU adapter.');
    }
    const device = await adapter.requestDevice();
    device.lost.then((info) => {
        console.error(`WebGPU device lost (${info.reason}): ${info.message}`);
        setStatus('webgpu-status', `Device lost: ${info.message}`, false);
    });
    const context = canvas.getContext('webgpu');
    if (!context) {
        throw new Error('Failed to acquire WebGPU canvas context.');
    }
    const format = navigator.gpu.getPreferredCanvasFormat();
    context.configure({
        device,
        format,
        alphaMode: 'opaque',
    });

    const shaderModule = device.createShaderModule({
        code: FENCE_WGSL,
    });

    const uniformBuffer = device.createBuffer({
        size: 16, // vec2<f32> center + vec2<f32> scale
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });

    const bindGroupLayout = device.createBindGroupLayout({
        entries: [
            {
                binding: 0,
                visibility: GPUShaderStage.VERTEX,
                buffer: { type: 'uniform' },
            },
        ],
    });

    const bindGroup = device.createBindGroup({
        layout: bindGroupLayout,
        entries: [
            {
                binding: 0,
                resource: { buffer: uniformBuffer },
            },
        ],
    });

    const pipeline = device.createRenderPipeline({
        layout: device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] }),
        vertex: {
            module: shaderModule,
            entryPoint: 'vs_main',
            buffers: [
                {
                    arrayStride: 8, // 2 * float32 (x, z)
                    attributes: [
                        {
                            shaderLocation: 0,
                            offset: 0,
                            format: 'float32x2',
                        },
                    ],
                },
            ],
        },
        fragment: {
            module: shaderModule,
            entryPoint: 'fs_main',
            targets: [{ format }],
        },
        primitive: {
            topology: 'line-list',
        },
    });

    let vertexBufferCapacity = 64 * 1024;
    let vertexBuffer = device.createBuffer({
        size: vertexBufferCapacity,
        usage: GPUBufferUsage.VERTEX | GPUBufferUsage.COPY_DST,
    });
    let vertexCount = 0;

    let boundsMinX = -250;
    let boundsMaxX = 250;
    let boundsMinZ = -250;
    let boundsMaxZ = 250;
    let camCenterX = 0;
    let camCenterZ = 0;
    let camHalfSpan = 250;
    let userCameraModified = false;

    function fitToBounds() {
        camCenterX = (boundsMinX + boundsMaxX) * 0.5;
        camCenterZ = (boundsMinZ + boundsMaxZ) * 0.5;
        const spanX = Math.max(boundsMaxX - boundsMinX, 10.0);
        const spanZ = Math.max(boundsMaxZ - boundsMinZ, 10.0);
        camHalfSpan = Math.max(spanX, spanZ) * 0.55;
        userCameraModified = false;
    }

    let dragging = false;
    let lastMouseX = 0;
    let lastMouseY = 0;

    canvas.addEventListener('mousedown', (e) => {
        if (e.button === 0) {
            dragging = true;
            lastMouseX = e.clientX;
            lastMouseY = e.clientY;
        }
    });

    window.addEventListener('mouseup', () => {
        dragging = false;
    });

    window.addEventListener('mousemove', (e) => {
        if (!dragging) return;
        const dx = e.clientX - lastMouseX;
        const dy = e.clientY - lastMouseY;
        lastMouseX = e.clientX;
        lastMouseY = e.clientY;

        const aspect = Math.max(canvas.width, 1) / Math.max(canvas.height, 1);
        const worldPerPixelX = (2.0 * camHalfSpan * aspect) / Math.max(canvas.width, 1);
        const worldPerPixelZ = (2.0 * camHalfSpan) / Math.max(canvas.height, 1);
        camCenterX -= dx * worldPerPixelX;
        camCenterZ += dy * worldPerPixelZ;
        userCameraModified = true;
    });

    canvas.addEventListener(
        'wheel',
        (e) => {
            e.preventDefault();
            const factor = e.deltaY > 0 ? 1.12 : 1.0 / 1.12;
            camHalfSpan = Math.min(Math.max(camHalfSpan * factor, 1.0), 100000.0);
            userCameraModified = true;
        },
        { passive: false }
    );

    canvas.addEventListener('dblclick', () => {
        fitToBounds();
    });

    function updateFences(segmentsXZ, fenceCount) {
        const validFences = Math.min(fenceCount, Math.floor(segmentsXZ.length / 4));
        vertexCount = validFences * 2;
        setText('fence-count', `${validFences}`);

        if (validFences === 0) {
            return;
        }

        let minX = Infinity;
        let maxX = -Infinity;
        let minZ = Infinity;
        let maxZ = -Infinity;

        const floatCount = validFences * 4;
        for (let i = 0; i < floatCount; i += 2) {
            const x = segmentsXZ[i];
            const z = segmentsXZ[i + 1];
            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (z < minZ) minZ = z;
            if (z > maxZ) maxZ = z;
        }

        if (Number.isFinite(minX) && Number.isFinite(maxX)) {
            boundsMinX = minX;
            boundsMaxX = maxX;
            boundsMinZ = minZ;
            boundsMaxZ = maxZ;
            if (!userCameraModified) {
                fitToBounds();
            }
        }

        const byteLength = floatCount * 4;
        if (byteLength > vertexBufferCapacity) {
            vertexBuffer.destroy();
            vertexBufferCapacity = Math.max(byteLength, vertexBufferCapacity * 2);
            vertexBuffer = device.createBuffer({
                size: vertexBufferCapacity,
                usage: GPUBufferUsage.VERTEX | GPUBufferUsage.COPY_DST,
            });
        }

        device.queue.writeBuffer(
            vertexBuffer,
            0,
            segmentsXZ.buffer,
            segmentsXZ.byteOffset,
            byteLength
        );
    }

    const uniformArray = new Float32Array(4);

    function renderFrame() {
        const aspect = Math.max(canvas.width, 1) / Math.max(canvas.height, 1);
        const scaleZ = 1.0 / Math.max(camHalfSpan, 0.001);
        const scaleX = scaleZ / aspect;

        uniformArray[0] = camCenterX;
        uniformArray[1] = camCenterZ;
        uniformArray[2] = scaleX;
        uniformArray[3] = scaleZ;
        device.queue.writeBuffer(uniformBuffer, 0, uniformArray);

        const encoder = device.createCommandEncoder();
        const pass = encoder.beginRenderPass({
            colorAttachments: [
                {
                    view: context.getCurrentTexture().createView(),
                    clearValue: { r: 0.08, g: 0.1, b: 0.16, a: 1.0 },
                    loadOp: 'clear',
                    storeOp: 'store',
                },
            ],
        });

        if (vertexCount > 0) {
            pass.setPipeline(pipeline);
            pass.setBindGroup(0, bindGroup);
            pass.setVertexBuffer(0, vertexBuffer);
            pass.draw(vertexCount, 1, 0, 0);
        }

        pass.end();
        device.queue.submit([encoder.finish()]);
        requestAnimationFrame(renderFrame);
    }

    requestAnimationFrame(renderFrame);
    setStatus('webgpu-status', `Ready (${format})`, true);

    return {
        gpu: navigator.gpu,
        adapter,
        device,
        context,
        format,
        canvas,
        updateFences,
    };
}

export function initWebSocket(onFenceData, url) {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const host = window.location.host || 'localhost:8080';
    const wsUrl = url ?? `${protocol}//${host}/ws`;
    const socket = new WebSocket(wsUrl);
    socket.binaryType = 'arraybuffer';
    socket.addEventListener('open', () => {
        console.log('[WS] Connected to', wsUrl);
        setStatus('ws-status', `Connected (${wsUrl})`, true);
        socket.send('hello from srr2 webgpu client');
    });
    socket.addEventListener('message', (event) => {
        if (typeof event.data === 'string') {
            console.log('[WS] Text message:', event.data);
            setText('ws-last-msg', event.data);
        }
        else if (event.data instanceof ArrayBuffer) {
            if (event.data.byteLength >= 8) {
                const view = new DataView(event.data);
                const magic = view.getUint32(0, true);
                if (magic === FENCE_MAGIC) {
                    const fenceCount = view.getUint32(4, true);
                    const availableFloats = Math.floor((event.data.byteLength - 8) / 4);
                    const segmentsXZ = new Float32Array(event.data, 8, availableFloats);
                    onFenceData(segmentsXZ, fenceCount);
                    setText('ws-last-msg', `Fence update: ${fenceCount} segments (${event.data.byteLength} B)`);
                    return;
                }
            }
            setText('ws-last-msg', `Binary (${event.data.byteLength} B)`);
        }
    });
    socket.addEventListener('error', (event) => {
        console.error('[WS] Error:', event);
        setStatus('ws-status', 'Error', false);
    });
    socket.addEventListener('close', (event) => {
        console.log(`[WS] Closed (code=${event.code}, reason=${event.reason})`);
        setStatus('ws-status', `Closed (${event.code})`, false);
    });
    return {
        socket,
        url: wsUrl,
        sendText(message) {
            if (socket.readyState === WebSocket.OPEN) {
                socket.send(message);
            }
        },
        sendBinary(data) {
            if (socket.readyState === WebSocket.OPEN) {
                socket.send(data);
            }
        },
        close() {
            socket.close();
        },
    };
}

export async function bootstrap() {
    const canvas = document.getElementById('gpu-canvas');
    if (!canvas) {
        throw new Error('Canvas element #gpu-canvas not found.');
    }
    canvas.width = window.innerWidth;
    canvas.height = window.innerHeight;
    window.addEventListener('resize', () => {
        canvas.width = window.innerWidth;
        canvas.height = window.innerHeight;
    });

    let pendingFences = null;
    let webgpuHandle = null;

    const ws = initWebSocket((segments, count) => {
        if (webgpuHandle) {
            webgpuHandle.updateFences(segments, count);
        } else {
            pendingFences = { segments, count };
        }
    });

    const webgpu = await initWebGPU(canvas);
    webgpuHandle = webgpu;

    if (pendingFences !== null) {
        webgpu.updateFences(pendingFences.segments, pendingFences.count);
        pendingFences = null;
    }

    const handles = { webgpu, ws };
    window.srr2 = handles;
    return handles;
}

bootstrap().catch((err) => {
    console.error('Initialization failed:', err);
    const message = err instanceof Error ? err.message : String(err);
    setStatus('webgpu-status', message, false);
});
