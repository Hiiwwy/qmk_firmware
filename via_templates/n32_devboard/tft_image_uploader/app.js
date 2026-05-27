const VID = 0x1fc9;
const PID = 0x0487;
const PACKET_SIZE = 32;
const CMD = 0x7d;

const SUB = {
  INFO: 0x00,
  BEGIN: 0x01,
  DATA: 0x02,
  END: 0x03,
  ABORT: 0x04,
  SHOW: 0x05,
  CLEAR: 0x06,
};

const STATUS = {
  0x00: "OK",
  0x01: "Busy",
  0x02: "Range or size error",
  0x03: "Sequence/order error",
  0x04: "Flash write error",
  0x05: "Invalid image or command",
};

const connectButton = document.querySelector("#connectButton");
const uploadButton = document.querySelector("#uploadButton");
const showButton = document.querySelector("#showButton");
const clearButton = document.querySelector("#clearButton");
const fileInput = document.querySelector("#fileInput");
const deviceStatus = document.querySelector("#deviceStatus");
const capacityStatus = document.querySelector("#capacityStatus");
const imageStatus = document.querySelector("#imageStatus");
const payloadStatus = document.querySelector("#payloadStatus");
const progress = document.querySelector("#progress");
const progressText = document.querySelector("#progressText");
const logEl = document.querySelector("#log");

let device = null;
let reportId = 0;
let pendingReport = null;
let info = {
  capacity: 57344,
  payloadSize: 25,
};

function log(message) {
  const time = new Date().toLocaleTimeString();
  logEl.textContent += `[${time}] ${message}\n`;
  logEl.scrollTop = logEl.scrollHeight;
}

function setBusy(isBusy) {
  connectButton.disabled = isBusy;
  uploadButton.disabled = isBusy || !device || !fileInput.files.length;
  showButton.disabled = isBusy || !device;
  clearButton.disabled = isBusy || !device;
}

function putU32LE(packet, offset, value) {
  packet[offset + 0] = value & 0xff;
  packet[offset + 1] = (value >> 8) & 0xff;
  packet[offset + 2] = (value >> 16) & 0xff;
  packet[offset + 3] = (value >> 24) & 0xff;
}

function getU32LE(data, offset) {
  return (
    data[offset + 0] |
    (data[offset + 1] << 8) |
    (data[offset + 2] << 16) |
    (data[offset + 3] << 24)
  ) >>> 0;
}

function getU16LE(data, offset) {
  return data[offset + 0] | (data[offset + 1] << 8);
}

function makePacket(subcmd) {
  const packet = new Uint8Array(PACKET_SIZE);
  packet[0] = CMD;
  packet[1] = subcmd;
  return packet;
}

function onInputReport(event) {
  if (!pendingReport) {
    return;
  }

  const bytes = new Uint8Array(event.data.buffer);
  if (bytes[0] !== CMD) {
    return;
  }

  const { resolve, timeout } = pendingReport;
  clearTimeout(timeout);
  pendingReport = null;
  resolve(bytes);
}

async function sendPacket(packet) {
  if (!device) {
    throw new Error("Device is not connected");
  }
  if (pendingReport) {
    throw new Error("A HID request is already pending");
  }

  const responsePromise = new Promise((resolve, reject) => {
    const timeout = setTimeout(() => {
      pendingReport = null;
      reject(new Error("Timed out waiting for keyboard response"));
    }, 2000);
    pendingReport = { resolve, reject, timeout };
  });

  await device.sendReport(reportId, packet);
  const response = await responsePromise;
  const status = response[2];
  if (status !== 0) {
    throw new Error(STATUS[status] || `Status 0x${status.toString(16)}`);
  }
  return response;
}

async function refreshInfo() {
  const response = await sendPacket(makePacket(SUB.INFO));
  info.capacity = getU32LE(response, 3);
  info.uploadSize = getU32LE(response, 7);
  info.uploadOffset = getU32LE(response, 11);
  info.payloadSize = response[15] || 25;
  info.frameCount = response[16];
  info.width = getU16LE(response, 17);
  info.height = getU16LE(response, 19);

  capacityStatus.textContent = `${info.capacity} bytes`;
  payloadStatus.textContent = `${info.payloadSize} bytes`;
  imageStatus.textContent = info.frameCount
    ? `${info.width}x${info.height}, ${info.frameCount} frame(s)`
    : "Empty";
}

async function connect() {
  if (!("hid" in navigator)) {
    throw new Error("WebHID is not available in this browser");
  }

  const devices = await navigator.hid.requestDevice({
    filters: [{ vendorId: VID, productId: PID }],
  });
  if (!devices.length) {
    return;
  }

  device = devices[0];
  await device.open();
  device.addEventListener("inputreport", onInputReport);

  reportId = 0;
  deviceStatus.textContent = device.productName || "N32 Devboard";
  connectButton.textContent = "Connected";
  setBusy(false);
  await refreshInfo();
  log("Connected");
}

async function uploadSelectedFile() {
  const file = fileInput.files[0];
  if (!file) {
    return;
  }
  if (file.size > info.capacity) {
    throw new Error(`File is ${file.size} bytes, capacity is ${info.capacity} bytes`);
  }

  setBusy(true);
  progress.value = 0;
  progressText.textContent = "Reading file";

  const bytes = new Uint8Array(await file.arrayBuffer());
  const begin = makePacket(SUB.BEGIN);
  putU32LE(begin, 2, bytes.length);
  await sendPacket(begin);

  log(`Upload started: ${file.name}, ${bytes.length} bytes`);

  let offset = 0;
  while (offset < bytes.length) {
    const chunkLength = Math.min(info.payloadSize, bytes.length - offset);
    const packet = makePacket(SUB.DATA);
    putU32LE(packet, 2, offset);
    packet[6] = chunkLength;
    packet.set(bytes.subarray(offset, offset + chunkLength), 7);

    await sendPacket(packet);
    offset += chunkLength;

    const percent = Math.floor((offset * 100) / bytes.length);
    progress.value = percent;
    progressText.textContent = `${offset}/${bytes.length} bytes`;
  }

  await sendPacket(makePacket(SUB.END));
  progress.value = 100;
  progressText.textContent = "Upload complete";
  log("Upload complete");
  await refreshInfo();
  setBusy(false);
}

async function showImage() {
  await sendPacket(makePacket(SUB.SHOW));
  log("Show image command sent");
}

async function clearImage() {
  await sendPacket(makePacket(SUB.CLEAR));
  log("Image cleared");
  await refreshInfo();
}

connectButton.addEventListener("click", async () => {
  try {
    setBusy(true);
    await connect();
  } catch (error) {
    log(error.message);
    setBusy(false);
  }
});

fileInput.addEventListener("change", () => {
  uploadButton.disabled = !device || !fileInput.files.length;
});

uploadButton.addEventListener("click", async () => {
  try {
    await uploadSelectedFile();
  } catch (error) {
    log(error.message);
    try {
      await sendPacket(makePacket(SUB.ABORT));
    } catch (_) {
      // Ignore abort errors.
    }
    setBusy(false);
  }
});

showButton.addEventListener("click", async () => {
  try {
    await showImage();
  } catch (error) {
    log(error.message);
  }
});

clearButton.addEventListener("click", async () => {
  try {
    await clearImage();
  } catch (error) {
    log(error.message);
  }
});

setBusy(false);
