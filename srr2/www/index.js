import * as THREE from 'https://cdn.jsdelivr.net/npm/three@0.187.0/+esm'
import { OrbitControls } from 'https://cdn.jsdelivr.net/npm/three@0.187.0/addons/controls/OrbitControls.js/+esm'

const adapter = await navigator.gpu.requestAdapter()
const device = await adapter.requestDevice()
const canvas = document.querySelector('canvas')
const context = canvas.getContext('webgpu')
const format = navigator.gpu.getPreferredCanvasFormat()
context.configure({ device, format })

canvas.width = window.innerWidth
canvas.height = window.innerHeight

const cameraData = new Float32Array(32)
const camera = new THREE.PerspectiveCamera(75., canvas.width / canvas.height, .1, 100_000.)
camera.position.set(0., 1_000., 0.)
camera.lookAt(0., 0., 0.)
const control = new OrbitControls(camera, canvas)
control.target.set(0., 0., 0.)

let vertexCount = 2
const lines = [ 1., 0.,  0., 0x00ff00ff,
               -1., 0.,  0., 0x00ff00ff, ]
const vertexData = new ArrayBuffer(lines.length * 4)
const vertexView = new DataView(vertexData)
for (let i = 0; i < lines.length; ++i) {
    if ((i + 1) % 4 == 0) vertexView.setUint32(i * 4, lines[i], true)
    else vertexView.setFloat32(i * 4, lines[i], true)
}

const vertexBuffer = device.createBuffer({
    label: 'lines',
    size: 1_00_000 * 4 * 4,
    usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.VERTEX
})
device.queue.writeBuffer(vertexBuffer, 0, vertexData)

const uniformBuffer = device.createBuffer({
    label: 'uniform',
    size: 64 * 64,
    usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.UNIFORM
})

const module = device.createShaderModule({ code: `
    struct VertOut { @builtin(position) pos: vec4f, @location(0) color: vec4f };
    struct Uniform { View: mat4x4f, Projection: mat4x4f };

    @group(0) @binding(0) var<uniform> uniforms: Uniform;

    @vertex fn vs_main(@location(0) pos: vec3f, @location(1) color: u32) -> VertOut {
        var out: VertOut;
        out.pos = uniforms.Projection * uniforms.View * vec4f(pos.x, pos.y, -pos.z, 1.);
        out.color = unpack4x8unorm(color).bgra;
        return out;
    }
    @fragment fn fs_main(out: VertOut) -> @location(0) vec4f {
        return out.color;
    }
`})

const bindGroupLayout = device.createBindGroupLayout({
    entries: [{
        binding: 0,
        visibility: GPUShaderStage.VERTEX,
        buffer: { type: 'uniform' }
    }]
})

const bindGroup = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [{ binding: 0, resource: { buffer: uniformBuffer }}]
})

const pipeline = device.createRenderPipeline({
    layout: device.createPipelineLayout({ bindGroupLayouts: [ bindGroupLayout ]}),
    vertex: {
        module,
        entryPoint: 'vs_main',
        buffers: [{
            arrayStride: 4*3 + 4,
            attributes: [
                { shaderLocation: 0, offset: 0, format: 'float32x3' },
                { shaderLocation: 1, offset: 4*3, format: 'uint32' },
            ]
        }]
    },
    fragment: {
        module,
        entryPoint: 'fs_main',
        targets: [{ format }]
    },
    primitive: {
        topology: 'line-list'
    }
})

const renderPassDescriptor = {
    colorAttachments: [{
        loadOp: 'clear',
        storeOp: 'store',
        clearValue: [ .1, .1, .1, 1. ],
        view: undefined
    }]
}

function frame() {
    control.update()
    cameraData.set(camera.matrixWorldInverse.elements, 0)
    cameraData.set(camera.projectionMatrix.elements, 16)
    device.queue.writeBuffer(uniformBuffer, 0, cameraData)

    renderPassDescriptor.colorAttachments[0].view = context.getCurrentTexture().createView()
    const encoder = device.createCommandEncoder()
    const pass = encoder.beginRenderPass(renderPassDescriptor)

    pass.setPipeline(pipeline)
    pass.setBindGroup(0, bindGroup)
    pass.setVertexBuffer(0, vertexBuffer)
    pass.draw(vertexCount)
    pass.end()

    device.queue.submit([ encoder.finish() ])
    
    requestAnimationFrame(frame)
}

requestAnimationFrame(frame)

const socket = new WebSocket(`ws://${location.hostname}:9001`)
socket.binaryType = 'arraybuffer'
socket.onmessage = e => {
    if (e.data instanceof ArrayBuffer) {
        const view = new DataView(e.data)
        vertexCount = view.getUint32(0, true)
        device.queue.writeBuffer(vertexBuffer, 0, e.data.slice(4))
    }
}
socket.onerror = err => {
    console.log('error: ', err)
}
