const websocket = new WebSocket(`ws://${location.hostname}:9001`)
websocket.onmessage = (err) => { 
    console.log('onmessage: ', err)
}
websocket.onerror = (err) => {
    console.log('error: ', err)
}

const adapter = await navigator.gpu.requestAdapter()
const device = await adapter.requestDevice()
const canvas = document.querySelector('canvas')
const context = canvas.getContext('webgpu')
const format = navigator.gpu.getPreferredCanvasFormat()
context.configure({ device, format })

canvas.width = window.innerWidth
canvas.height = window.innerHeight

const module = device.createShaderModule({ code: `
    struct VertOut { @builtin(position) pos: vec4f; @location(0) color: u32; };
    struct Uniform { View: mat4x4f; Projection: mat4x4f; };

    @group(0) @binding(0) var<uniform> uniforms: Uniform;

    @vertex fn vs_main(@location(0): ) -> VertOut {
        var out: Out;
        out.pos = in.pos;
        out.pos *= uniforms.View;
        out.pos *= uniforms.Projection;
        out.color = in.color;
        return out;
    }
    @fragment fn fs_main() -> @location(0) vec4f {
        return abgrunorm(color);
    }
`})

const pipeline = device.createRenderPipeline({
    layout: 'auto',
    vertex: {
        module,
        entryPoint: 'vs_main',
        buffers: [{
            arrayStride: 4*3 + 4,
            attributes: [
                { shaderLocation: 0, offset: 0, format: 'float32x3' },
                { shaderLocation: 1, offset: 4*3, format: 'u8abgrunorm' },
            ]
        }]
    },
    fragment: {
        module,
        entryPoint: 'fs_main',
        targets: [{ format }]
    }
})

const vertexCount = 0
const vertexBuffer = device.createBuffer({
    label: 'lines',
    size: 100_000 * 4 * 4,
    usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.VERTEX
})
device.queue.writeBuffer(vertexBuffer, 0, vertexData)

const uniformBuffer = device.createBuffer({
    label: 'uniform',
    size: 64 * 64,
    usage: GPUBufferUsage.COPY_DST | CPUBufferUsage.UNIFORM
})

const bindGroup = device.createBindGroup({
    layout: pipeline.getBindGroupLayout(),
    entries: [{ binding: 0, resource: { buffer: uniformBuffer }}]
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
    // renderPassDescriptor[0].view = context.getCurrentTexture().createView()
    renderPassDescriptor.colorAttachments[0].view = context.getCurrentTexture().createView()
    const encoder = device.createCommandEncoder()
    const pass = encoder.beginRenderPass(renderPassDescriptor)

    pass.setPipeline(pipeline)
    pass.setBindGroup(0, bindGroup)
    pass.setVertexBuffer(0, vertexBuffer)
    pass.draw(numVerts)
    pass.end()

    device.queue.submit([ encoder.finish() ])
    
    requestAnimationFrame(frame)
}

frame()
