#!/usr/bin/env bun run

// Bun JavaScript program that runs the `miniogg` program,
// sends its binary output to the Deepgram API,
// and prints each incoming Deepgram JSON text line

const params = new URLSearchParams({
  model: "nova-2-general",
  filler_words: true,
  interim_results: true,
  punctuation: true,
  smart_format: true,
//  vad_events: true,
//  diarize: true
})

const url = `wss://api.deepgram.com/v1/listen?${params}`
const socket = new WebSocket(url, { headers: {
  Authorization: `Token ${process.env.DEEPGRAM_API_KEY}`
}})

socket.onopen = async () => {
  try {
    console.warn("deepgram open")
    const homePath = process.env.HOME
    const socketPath = `${homePath}/minirec.sock`
    const miniogg = Bun.spawn(["miniogg", socketPath])
    console.warn("miniogg open")
    for await (const chunk of miniogg.stdout) {
      socket.send(chunk)
    }
  } catch (error) {
    console.error(error)
    process.exit(1)
  }
}

socket.onmessage = (event) => {
  console.log(event.data)
}

socket.onclose = (event) => {
  console.warn("deepgram closed")
}
  
