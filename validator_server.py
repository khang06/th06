import asyncio
import os
import struct
import sys

total = 0
replay_queue: asyncio.Queue[str] = asyncio.Queue()


async def handler(reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
    while True:
        replay = await replay_queue.get()
        # print(f"({total - replay_queue.qsize()}/{total}) {replay}")

        writer.write(replay.encode())
        await writer.drain()

        ret_bytes = await reader.readexactly(12)
        stage, expected, actual = struct.unpack("<iii", ret_bytes)
        if stage > 0:
            print(
                f"Desync on stage {stage} in {replay}! Expected {expected}, got {actual}"
            )
        elif stage == -1:
            print(f"Failed to load {replay}!")

        replay_queue.task_done()


async def main():
    global total

    if len(sys.argv) != 3:
        print("Usage: validator_server.py <replay folder> <port>")
        sys.exit(1)

    print("Getting replays...")
    for replay in os.listdir(sys.argv[1]):
        await replay_queue.put(os.path.join(sys.argv[1], replay))
    total = replay_queue.qsize()

    print("Starting server...")
    server = await asyncio.start_server(handler, "127.0.0.1", port=int(sys.argv[2]))

    print("Waiting for clients...")
    await replay_queue.join()

    server.close()
    await server.wait_closed()

    print("Done!")


if __name__ == "__main__":
    asyncio.run(main())
