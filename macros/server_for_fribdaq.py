import sys
import socketserver

def eprint(*args, **kwargs):
  print(*args, file=sys.stderr,**kwargs)

def checkNumber(number):
  number = int(number)
  
  if number < 0:
    raise ValueError()

class ServerForFRIBDAQ():
  def __init__(self, ctrl, tabs):
    HOST, PORT = "localhost", 41234

    self.server = socketserver.TCPServer((HOST, PORT), MessageHandler)
    self.server.ctrl = ctrl
    self.server.tabs = tabs

    print("== Starting the FRIBDAQ service server!")
    self.server.serve_forever()

class MessageHandler(socketserver.BaseRequestHandler):
  def setup(self):
    print("== Connection established")
    self.info("== Connection established")

  def respond(self, level, message):
    message = level + "\n" + message + "\n"
    self.request.send(message.encode("utf-8"))

  def error(self, message):
    self.respond("error", message)

  def info(self, message):
    self.respond("info", message)

  def handle(self):
    while True:
      try:
        data = self.request.recv(1024).decode("utf-8").strip()
        ctrl = self.server.ctrl
        tabs = self.server.tabs

        if not data:
          print("== Client disconnected")
          break

        if data.startswith("list") or data.startswith("help"):
          message = "== Command list:\n"
          message += "     list, help     - Show this message\n"
          message += "     begin          - Click the start DAQ button on Janus GUI\n"
          message += "     end            - Click the stop DAQ button on Janus GUI\n"
          message += "     sid     SID    - Set the source ID to SID\n"
          message += "     title   TITLE  - Set the title to TITLE\n"
          message += "     run     RUNNO  - Set the run number to RUNNO\n"
          message += "     ringon         - Turn on RingBuffer output\n"
          message += "     ringoff        - Turn off RingBuffer output\n"
          message += "     ring    RING   - Set the output RingBuffer to RING\n"
          message += "     exit           - Disconnect from the server and quit\n"
          self.info(message)

        elif data.startswith("title"):
          title = data.replace("title ", "")
          self.info(f"== Setting title to {title}")

          tabs.par_def_svar["RunTitle"].set(title)

        elif data.startswith("sid"):
          sid = data.replace("sid ", "")

          try:
            checkNumber(sid)

            self.info(f"== Setting source ID to {sid}")

            tabs.par_def_svar["SourceID"].set(sid)
          except ValueError:
            self.error(f"== Only allowed positive integer source ID: {sid}")

        elif data.startswith("run"):
          runno = data.replace("run ", "")

          try:
            checkNumber(runno)

            self.info(f"== Setting run number to {runno}")

            ctrl.RunNumber.set(runno)
          except ValueError:
            self.error(f"== Only allowed positive integer run number: {runno}")

        elif data.startswith("ringon"):
          self.info(f"== RingBuffer output ON")

          tabs.par_def_svar["RingBuffer"].set(True)

        elif data.startswith("ringoff"):
          self.info(f"== RingBuffer output OFF")

          tabs.par_def_svar["RingBuffer"].set(False)

        elif data.startswith("ring"):
          ring = data.replace("ring ", "")
          self.info(f"== Setting output RingBuffer to {ring}")

          tabs.par_def_svar["RingBufferName"].set(ring)

        elif data == "begin":
          if ctrl.plugged.get() == 1:
            if ctrl.b_apply['state'] != 'disabled':
              print("== Saving setting changes before starting!")
              self.info("== Saving setting changes before starting!")
              ctrl.b_apply.invoke()

            message = "\n"
            message += "====== FRINBDAQ-related run information ======\n"
            message += "      Ring: " + tabs.par_def_svar["RingBufferName"].get() + " (" + ("ON" if tabs.par_def_svar["RingBuffer"].get() else "OFF") + ")\n"
            message += " Source ID: " + tabs.par_def_svar["SourceID"].get() + "\n"
            message += "      Run#: " + ctrl.RunNumber.get() + "\n"
            message += "     Title: " + tabs.par_def_svar["RunTitle"].get() + "\n"
            message += "==============================================\n"
            message += "\n"
            message += "== Beginning the run!\n"
            self.info(message)
            print(message)

            ctrl.bstart.invoke()
          else:
            eprint("== Janus is not connected to or disconnected from the board(s)!!!")
            self.error("== Janus is not connected to or disconnected from the board(s)!!!")

        elif data == "end":
          if ctrl.plugged.get() == 1:
            print("== Ending the run!")
            self.info("== Ending the run!")
            ctrl.bstop.invoke()
          else:
            eprint("== Janus is not connected to or disconnected from the board(s)!!!")
            self.error("== Janus is not connected to or disconnected from the board(s)!!!")

        else:
          eprint(f"== Unknown command: {data}")
          self.error(f"== Unknown command: {data}")
      except ConnectionResetError:
        break
