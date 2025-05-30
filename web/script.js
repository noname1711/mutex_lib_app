// Biến toàn cục
const SERVER_PORT = 8080; // Phải khớp với SERVER_PORT trong common.h
let clients = {};
let mutexes = [];
let activeMessages = [];
let lastMessages = {};

// Khởi tạo ứng dụng
function init() {
    console.log("Initializing Mutex Monitor...");
    updateData();
    setInterval(updateData, 1000);
    window.addEventListener('resize', updateAllPositions);
}

// Cập nhật tất cả vị trí
function updateAllPositions() {
    updateArrowPositions();
}

// Cập nhật dữ liệu từ server
function updateData() {
    fetch(`http://localhost:${SERVER_PORT + 1}/mutexes`)
        .then(response => {
            if (!response.ok) throw new Error('Network response was not ok');
            return response.json();
        })
        .then(data => {
            processMutexData(data.mutexes);
            updateClients();
            updateMutexList();
        })
        .catch(error => {
            console.error('Error fetching data:', error);
        });
}

// Xử lý dữ liệu mutex
function processMutexData(newMutexes) {
    newMutexes.forEach(mutex => {
        if (mutex.last_message && mutex.last_message !== '' && 
            (!lastMessages[mutex.name] || lastMessages[mutex.name] !== mutex.last_message)) {
            
            showMessageAnimation(mutex.owner, mutex.last_message);
            lastMessages[mutex.name] = mutex.last_message;
        }
    });
    mutexes = newMutexes;
}

// Cập nhật danh sách client
function updateClients() {
    const clientsContainer = document.getElementById('clients');
    if (!clientsContainer) return;

    // Tìm client active
    const activePids = new Set();
    mutexes.forEach(mutex => {
        if (mutex.owner > 0) activePids.add(mutex.owner);
    });

    // Thêm client mới
    activePids.forEach(pid => {
        if (!clients[pid]) {
            const client = document.createElement('div');
            client.className = 'client';
            client.id = `client-${pid}`;
            client.innerHTML = `Client<br>PID: ${pid}`;
            clientsContainer.appendChild(client);
            clients[pid] = client;
        }
    });

    // Xóa client không còn active
    Object.keys(clients).forEach(pid => {
        if (!activePids.has(parseInt(pid))) {
            const clientElement = document.getElementById(`client-${pid}`);
            if (clientElement) {
                clientElement.remove();
            }
            delete clients[pid];
        }
    });

    // Cập nhật trạng thái lock
    Object.keys(clients).forEach(pid => {
        const isLocked = mutexes.some(m => m.owner == pid && m.locked);
        clients[pid].className = isLocked ? 'client locked' : 'client';
    });
}

// Cập nhật danh sách mutex
function updateMutexList() {
    const container = document.getElementById('mutex-items');
    if (!container) return;

    container.innerHTML = '';
    
    mutexes.forEach(mutex => {
        const item = document.createElement('div');
        item.className = mutex.locked ? 'mutex-item locked' : 'mutex-item';
        
        item.innerHTML = `
            <div>
                <span class="mutex-name">${mutex.name}</span>
                <div>Owner: ${mutex.owner > 0 ? 'PID ' + mutex.owner : 'None'}</div>
            </div>
            <div class="mutex-status ${mutex.locked ? 'status-locked' : 'status-unlocked'}">
                ${mutex.locked ? 'LOCKED' : 'UNLOCKED'}
            </div>
        `;
        
        container.appendChild(item);
    });
}

// Hiển thị animation message
function showMessageAnimation(pid, message) {
    console.log(`Showing message from PID ${pid}: ${message}`);
    
    const clientElement = document.getElementById(`client-${pid}`);
    if (!clientElement) {
        console.error(`Client element for PID ${pid} not found!`);
        return;
    }
    
    // Tạo mũi tên
    const arrow = document.createElement('div');
    arrow.className = 'message-arrow';
    arrow.textContent = `➔ ${message.substring(0, 20)}${message.length > 20 ? '...' : ''}`;
    document.body.appendChild(arrow);
    
    // Lưu thông tin
    const messageId = Date.now();
    activeMessages.push({
        id: messageId,
        element: arrow,
        pid: pid
    });
    
    updateArrowPositions();
    
    // Tự động xóa sau 3s
    setTimeout(() => {
        arrow.remove();
        activeMessages = activeMessages.filter(m => m.id !== messageId);
    }, 3000);
}

// Cập nhật vị trí mũi tên
function updateArrowPositions() {
    const server = document.getElementById('server');
    if (!server) return;

    const serverRect = server.getBoundingClientRect();
    const serverX = serverRect.left + serverRect.width / 2;
    const serverY = serverRect.top + serverRect.height / 2;
    
    activeMessages.forEach(msg => {
        const clientElement = document.getElementById(`client-${msg.pid}`);
        if (!clientElement || !msg.element) return;
        
        const clientRect = clientElement.getBoundingClientRect();
        const clientX = clientRect.left + clientRect.width / 2;
        const clientY = clientRect.top + clientRect.height / 2;
        
        const angle = Math.atan2(serverY - clientY, serverX - clientX);
        const length = Math.sqrt(Math.pow(serverX - clientX, 2) + Math.pow(serverY - clientY, 2));
        
        Object.assign(msg.element.style, {
            left: `${clientX}px`,
            top: `${clientY}px`,
            width: `${length}px`,
            transform: `rotate(${angle}rad)`
        });
    });
}

// Khởi chạy khi trang tải xong
window.addEventListener('load', init);