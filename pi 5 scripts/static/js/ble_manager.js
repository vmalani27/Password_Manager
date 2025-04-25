class BLEManager {
    constructor() {
        this.device = null;
        this.characteristic = null;
        this.isConnected = false;
        this.SERVICE_UUID = '4fafc201-1fb5-459e-8fcc-c5c9c331914b';
        this.CHARACTERISTIC_UUID = 'beb5483e-36e1-4688-b7f5-ea07361b26a8';
    }

    async connect() {
        try {
            this.device = await navigator.bluetooth.requestDevice({
                filters: [
                    { namePrefix: 'ESP32' }
                ],
                optionalServices: [this.SERVICE_UUID]
            });

            const server = await this.device.gatt.connect();
            const service = await server.getPrimaryService(this.SERVICE_UUID);
            this.characteristic = await service.getCharacteristic(this.CHARACTERISTIC_UUID);
            
            this.isConnected = true;
            this.updateStatus('Connected to ESP32');
            return true;
        } catch (error) {
            console.error('Connection error:', error);
            this.updateStatus('Connection failed: ' + error.message);
            return false;
        }
    }

    async connectToDevice(address) {
        try {
            this.device = await navigator.bluetooth.requestDevice({
                filters: [
                    { address: address }
                ],
                optionalServices: [this.SERVICE_UUID]
            });

            const server = await this.device.gatt.connect();
            const service = await server.getPrimaryService(this.SERVICE_UUID);
            this.characteristic = await service.getCharacteristic(this.CHARACTERISTIC_UUID);
            
            this.isConnected = true;
            this.updateStatus('Connected to device');
            return true;
        } catch (error) {
            console.error('Connection error:', error);
            this.updateStatus('Connection failed: ' + error.message);
            return false;
        }
    }

    async disconnect() {
        if (this.device && this.device.gatt.connected) {
            await this.device.gatt.disconnect();
            this.isConnected = false;
            this.updateStatus('Disconnected from device');
        }
    }

    async sendCommand(command) {
        if (!this.isConnected) {
            this.updateStatus('Not connected to device');
            return false;
        }

        try {
            const encoder = new TextEncoder();
            const data = encoder.encode(command);
            await this.characteristic.writeValue(data);
            this.updateStatus('Command sent successfully');
            return true;
        } catch (error) {
            console.error('Send error:', error);
            this.updateStatus('Failed to send command: ' + error.message);
            return false;
        }
    }

    updateStatus(message) {
        const statusElement = document.getElementById('bleStatus');
        const notificationElement = document.querySelector('.notification-area');
        
        if (statusElement) {
            statusElement.textContent = this.isConnected ? 'Connected' : 'Disconnected';
            statusElement.className = this.isConnected ? 'connected' : '';
        }
        
        if (notificationElement) {
            notificationElement.textContent = message;
        }
    }
}

// Export the BLEManager class
window.BLEManager = BLEManager; 