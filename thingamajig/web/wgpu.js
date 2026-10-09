export default class Wgpu {
    constructor() {
        this.inited = false
        this.device = null
        this.canvas = null
        this.context = null
        this.canvasFormat = null
        this.pipeline = null
        this.passDescriptor = null
        this.vertexBuffer = null
    }
    async init() {
        const adapter = await navigator.gpu.requestAdapter()
        this.device = await adapter.requestDevice()
        this.canvas = document.querySelector('canvas')
        this.context = this.canvas.getContext('webgpu')
        this.canvasFormat = navigator.gpu.getPreferredCanvasFormat()
        this.context.configure({ device: this.device, format: this.canvasFormat })
        this.vsShader = this.device.createShaderModule({ code: `
            @vertex fn main(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
                var pos = array<vec2f, 3>(vec2f(.5, .5), vec2f(-.5, -.5), vec2f(.5, -.5));
                return vec4f(pos[i], 0., 1.);
            }` })
        this.fsShader = this.device.createShaderModule({ code: `
            @fragment fn main() -> @location(0) vec4f {
                return vec4f(1., 0., 0., 1.);
            }` })
        this.pipeline = this.device.createRenderPipeline({
            layout: 'auto',
            vertex: { module: this.vsShader },
            fragment: { module: this.fsShader, targets: [{ format: this.canvasFormat }] },
        })
        this.passDescriptor = {
            colorAttachments: [{
                loadOp: 'clear',
                storeOp: 'store',
                clearValue: [ .1, .1, .1, 1. ],
                view: undefined,
            }],
        }
        this.inited = true
    }
    frame= () => {
        if (!this.inited) return

        this.passDescriptor.colorAttachments[0].view =
            this.context.getCurrentTexture().createView()
        const encoder = this.device.createCommandEncoder()
        const pass = encoder.beginRenderPass(this.passDescriptor)
        pass.setPipeline(this.pipeline)
        pass.draw(3)
        pass.end()

        this.device.queue.submit([ encoder.finish() ])

        requestAnimationFrame(this.frame)
    }
}
