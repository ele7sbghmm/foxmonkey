
export default class Wsock {
    constructor() {
        this.ws = new WebSocket(`ws://${location.hostname}:9000/ws`)
        this.ws.binaryType = 'arraybuffer'

        this.ws.onerror = () => { }
        this.ws.onmessage = (e) => {
            if (!(e.data instanceof ArrayBuffer))
                return
        }
    }
}
