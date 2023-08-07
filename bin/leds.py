from tkinter import *
        
class Led:   #  DNIN: We can inherit Widget from Tkinter to use its function and make the class more complete and powerfull
	def __init__(self, master, size=20):
		self.size = size
		self.master = master
		self.canvas = Canvas(self.master, width = self.size+4, height = self.size+4)
		self.ov1 = self.canvas.create_oval(2, 2, self.size-2, self.size-2, fill='grey', width=2, outline='grey')
		self.ov2 = self.canvas.create_oval(self.size/4, self.size/4, self.size/2, self.size/2, fill='white', width=0)

	def place(self, x, y):
		self.canvas.place(x = x, y = y)

	def rel_place(self, x, y):	# for relative placement
		self.canvas.place(relx=x, rely=y)

	def place_forget(self):
		self.canvas.place_forget()

	def set_color(self, color):
		if color == 'green':
			c1='green3'
			c2='green yellow'
		elif color == 'red':
			c1='red'
			c2='orange'
		elif color == 'yellow':
			c1='gold2'
			c2='yellow2'
		elif color == 'blue':
			c1='blue'
			c2='dodger blue'
		elif color == 'grey':	
			c1='grey80'
			c2='white'
		else:	
			c1='white'
			c2='white'
		self.canvas.itemconfig(self.ov1, fill=c1) 
		self.canvas.itemconfig(self.ov2, fill=c2) 


