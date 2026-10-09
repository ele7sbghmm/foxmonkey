import Wgpu from "./wgpu.js"
import Wsock from "./wsock.js"

const [ width, height ] = [ 640, 480 ]

const wgpu = new Wgpu()
await wgpu.init()
wgpu.frame()

const wsock = new Wsock()

window.wsock = wsock
window.wgpu = wgpu
