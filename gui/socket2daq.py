import socket
from threading import Thread, Lock
import time

import janus_shared as sh

class socket2daq:
	def __init__(self, host='', port=50017):
		self.host = host
		self.port = port
		self.rxrdy = False
		self.rxmsg = bytes('', encoding='utf-8')
		self.stopthread = False
		self.mutex = Lock()
		self.error = False

		self.connect()
		try:
			self.t = Thread(target=self.RX_thread)
			self.t.start()
		except:
			self.error = True
			raise("ERROR: Unable to start Server thread\n")


	def connect(self):
		self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
		try:
			self.s.bind((self.host, self.port))
		except socket.error as msg:
			self.error = True
			print("ERROR: Bind failed\n")
			raise (socket.error)
		self.s.listen(1)
		self.s.settimeout(5.0)
		self.conn, self.addr = self.s.accept()
		# Ensure the RX thread can exit promptly (recv must not block forever).
		try:
			self.conn.settimeout(0.5)
		except Exception:
			pass


	def dismiss(self):
		self.stopthread = True
		self.rxrdy = 0
		try:
			self.conn.shutdown(socket.SHUT_RDWR)
		except Exception:
			pass
		try:
			self.conn.close()
		except Exception:
			pass
		try:
			self.s.close()
		except Exception:
			pass
		try:
			if self.t.is_alive():
				self.t.join(timeout=1.0)
		except Exception:
			pass
			

	def RX_thread(self):
		rxbuff = bytes('', encoding='utf-8')
		wait_for_size = True
		msize = 0
		while not self.stopthread :
			self.mutex.acquire()
			# print("Received:", rxbuff)
			if self.rxrdy:
				self.mutex.release()
				time.sleep(0.1)
				continue
			else:
				self.mutex.release()
			if len(rxbuff) <= 1 or len(rxbuff) < msize:
				try:
					datain = self.conn.recv(1024)
				except socket.timeout:
					continue
				except socket.error as msg:
					self.error = True
					print(msg)
					try:
						self.conn.close()
					except Exception:
						pass
					try:
						self.s.close()
					except Exception:
						pass
					break
				if not datain:
					# Peer closed the connection.
					break
				rxbuff += datain
			if  wait_for_size and len(rxbuff) > 1:
				msize = rxbuff[0] + 256 * rxbuff[1]
				wait_for_size = False
			elif not wait_for_size and len(rxbuff) >= msize:
				self.mutex.acquire()
				self.rxmsg = rxbuff[2:msize] 
				self.rxrdy = True
				rxbuff = rxbuff[msize:]
				wait_for_size = True
				msize = 0
				self.mutex.release()


	def recv_data(self):
		self.mutex.acquire()
		if self.rxrdy:
			ret = self.rxmsg
			self.rxrdy = False
		else:	
			ret = bytes('', encoding='utf-8')
		self.mutex.release()
		return ret

	def send_cmd(self, cmd):
		dataout = bytes(cmd, encoding='utf-8')
		# print("Send:", dataout)
		try:
			self.conn.send(dataout)
		except socket.error as msg:
			self.error = True
			print("ERROR: send failed\n")
			self.s.close()
			#raise (socket.error)


# #####################################################################

SckConnected = False
SckError = False

def Open():
	global sock, SckConnected, SckError
	if SckConnected: return
	sock = socket2daq(port=50007)  # port number
	SckError = sock.error	#	Set SckError to the actual error in opening sock
	if not SckError: SckConnected = True
	# if sock.error: SckError = True
	# else: SckConnected = True

def SendCmd(cmd):
	global sock, SckConnected, SckError
	if not SckConnected: return
	sock.send_cmd(cmd)
	SckError = sock.error
	
def SendString(s):
	global sock, SckConnected, SckError
	if not SckConnected: return
	sock.send_cmd(s + '\n')
	SckError = sock.error

def GetString():
	global sock, SckConnected, SckError
	if not SckConnected: return ''
	cmsg = sock.recv_data()
	SckError = sock.error

	#1) UTF-8 strict
	try:
		return cmsg.decode('utf-8')
	except UnicodeDecodeError: 
		# 2) Common fallback into cp1252 or latin-1
		for enc in ("cp1252", "latin-1"):
			try:
				return cmsg.decode(enc)
			except UnicodeDecodeError:
				pass
		# 3) Last resort: decode with replacement characters for undecodable bytes
		return cmsg.decode('utf-8', errors='replace')

def GetData():
	global sock, SckConnected, SckError
	if not SckConnected: return ''
	return sock.recv_data()

def Close():
	global sock, SckConnected, SckError
	#if not SckConnected: return	
	sock.dismiss()
	SckConnected = False


