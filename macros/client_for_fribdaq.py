import socket
import sys
import threading

def process(message):
  lines = message.splitlines(True)
  if lines[0].startswith("info"):
    print("".join(lines[1:]))
  elif lines[0].startswith("error"):
    eprint("".join(lines[1:]))

def eprint(*args, **kwargs):
  print(*args, file=sys.stderr,**kwargs)

def start_client(host='localhost', port=41234):
  sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  try:
    sock.connect((host, port))
  except:
    eprint(f"== Unable to connect tp {host}:{port}. Check if the server is running!")
    exit()

  sock.settimeout(0.1)

  # Initial message
  while True:
    try:
      respond = sock.recv(1024).decode("utf-8").strip()
      if respond:
        process(respond)
      else:
        break
    except:
      break

  try:
    while True:
      command = input("Janus> ")
      if command == 'exit':
        break
      if command == '':
        continue
      sock.send(command.encode("utf-8"))

      # Response message
      while True:
        try:
          respond = sock.recv(1024).decode("utf-8").strip()
          if respond:
            process(respond)
          else:
            break
        except:
          break
  except KeyboardInterrupt:
    pass
  finally:
    sock.close()

if __name__ == "__main__":
  argc = len(sys.argv)
  argv = sys.argv

  if argc == 1:
    start_client()
  elif argc == 3:
    start_client(argv[1], int(argv[2]))
  else:
    eprint("== Wrong number of arguments!")
