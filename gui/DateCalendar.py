import tkinter as tk
from tkinter import ttk
import calendar
from datetime import datetime


class DateEntry(tk.Frame):
	"""Minimal DateEntry-like widget (no tkcalendar).
	Provides an Entry plus a dropdown button that opens a small
	month-view calendar popup to pick a date.
	The selected date is written into `textvariable` as yyyy-mm-dd.
	"""
	# Sizing + colors (tuned to resemble tkcalendar DateEntry)
	CELL_WIDTH = 3
	CELL_HEIGHT = 1
	BORDER_WIDTH = 1
	POPUP_BORDER_BG = "gray"
	HEADER_BG = "gray"
	DOW_BG = "#e6e6e6"
	GRID_BG = "white"
	CELL_BG = "white"
	CELL_BG_DISABLED = "#f0f0f0"
	CELL_BG_TODAY = "light yellow"
	CELL_BG_SELECTED = "light blue"
	ACTIVE_BG = "#f5f5f5"
	
	def __init__(
		self,
		parent,
		textvariable=None,
		year=None,
		month=None,
		day=None,
		date_pattern="yyyy-mm-dd",
	):
		super().__init__(parent)
		self.textvariable = textvariable
		self.date_pattern = date_pattern
		now = datetime.now()
		# Try to initialize from textvariable if it already contains a date
		initial_y, initial_m, initial_d = None, None, None
		if textvariable is not None:
			try:
				s = textvariable.get().strip()
				if s:
					y_s, m_s, d_s = s.split("-")
					initial_y, initial_m, initial_d = int(y_s), int(m_s), int(d_s)
			except Exception:
				pass
		self.year = year or initial_y or now.year
		self.month = month or initial_m or now.month
		self.day = day or initial_d or now.day
		# Use a standard ttk.Combobox as the field (integrated arrow, same height)
		self.entry = ttk.Combobox(self, textvariable=textvariable, width=12, state="readonly", values=())
		self.entry.pack(side=tk.LEFT, fill="x", expand=True)
		self.entry.bind("<Button-1>", lambda _e: (self.open_calendar(), "break")[1])
		self.entry.bind("<Down>", lambda _e: (self.open_calendar(), "break")[1])
		self.top = None
		self.set_date(self.year, self.month, self.day)


	def set_date(self, y, m, d):
		self.year = y
		self.month = m
		self.day = d
		if self.textvariable:
			self.textvariable.set(f"{y:04d}-{m:02d}-{d:02d}")


	def open_calendar(self):
		# Toggle close if already open
		if self.top is not None and self.top.winfo_exists():
			self.top.destroy()
			self.top = None
			return
		self.top = tk.Toplevel(self)
		self.top.wm_overrideredirect(True)
		self.top.bind("<Escape>", lambda _e: self._close_popup())
		self.top.bind("<FocusOut>", lambda _e: self._close_popup())
		x = self.entry.winfo_rootx()
		y = self.entry.winfo_rooty() + self.entry.winfo_height()
		self.top.geometry(f"+{x}+{y}")
		main = tk.Frame(self.top, bg=self.POPUP_BORDER_BG, bd=self.BORDER_WIDTH, relief="solid")
		main.pack()
		header = tk.Frame(main, bg=self.HEADER_BG)
		header.pack(fill="x")
		header.grid_columnconfigure(1, weight=1)
		header.grid_columnconfigure(4, weight=1)
		btn_style = dict(
			bg=self.HEADER_BG,
			fg="white",
			activebackground=self.HEADER_BG,
			activeforeground="white",
			relief="raised",
			bd=1,
			highlightthickness=1,
			padx=2,
			pady=0,
		)
		# Month navigation (separate)
		tk.Button(header, text="<", width=1, command=self.prev_month, **btn_style).grid(row=0, column=0, padx=1, pady=0)
		self.month_label = tk.Label(
			header,
			text=calendar.month_name[self.month],
			bg=self.HEADER_BG,
			fg="white",
			width=9,
			anchor="center",
			pady=0,
		)
		self.month_label.grid(row=0, column=1, padx=1, pady=0, sticky="ew")
		tk.Button(header, text=">", width=1, command=self.next_month, **btn_style).grid(row=0, column=2, padx=1, pady=0)
		# Year navigation (separate)
		tk.Button(header, text="<", width=1, command=self.prev_year, **btn_style).grid(row=0, column=3, padx=1, pady=0)
		self.year_label = tk.Label(
			header,
			text=str(self.year),
			bg=self.HEADER_BG,
			fg="white",
			width=6,
			anchor="center",
			pady=0,
		)
		self.year_label.grid(row=0, column=4, padx=1, pady=0, sticky="ew")
		tk.Button(header, text=">", width=1, command=self.next_year, **btn_style).grid(row=0, column=5, padx=1, pady=0)
		self.gridframe = tk.Frame(main, bg=self.GRID_BG)
		self.gridframe.pack()
		self.draw_calendar()
		# Ensure the popup gets focus
		try:
			self.top.focus_force()
		except Exception:
			pass
	
	
	def _close_popup(self):
		if self.top is not None and self.top.winfo_exists():
			self.top.destroy()
		self.top = None
	
	
	def on_year_change(self, *_):
		try:
			self.year = int(self.year_var.get())
		except Exception:
			return
		self.draw_calendar()
	
	
	def on_month_change(self, *_):
		try:
			selected = self.month_var.get()
			month_names = [calendar.month_name[i] for i in range(1, 13)]
			self.month = month_names.index(selected) + 1
		except Exception:
			return
		self.draw_calendar()
	
	
	def draw_calendar(self):
		if not hasattr(self, "gridframe"):
			return
		for w in self.gridframe.winfo_children():
			w.destroy()
		if hasattr(self, "month_label"):
			self.month_label.config(text=calendar.month_name[self.month])
		if hasattr(self, "year_label"):
			self.year_label.config(text=str(self.year))
		days = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
		for c, d in enumerate(days):
			tk.Label(
		    	self.gridframe,
		    	text=d,
		    	bg=self.DOW_BG,
		    	width=self.CELL_WIDTH,
		    	height=1,
		    	relief="solid",
		    	bd=self.BORDER_WIDTH,
			).grid(row=0, column=c, sticky="nsew")
		now = datetime.now()
		today_y, today_m, today_d = now.year, now.month, now.day
		month_weeks = calendar.monthcalendar(self.year, self.month)
		for r, week in enumerate(month_weeks, 1):
			for c, day in enumerate(week):
				if day == 0:
					tk.Label(
						self.gridframe,
						text="",
						width=self.CELL_WIDTH,
						height=self.CELL_HEIGHT,
						bg=self.CELL_BG_DISABLED,
						relief="solid",
						bd=self.BORDER_WIDTH,
					).grid(row=r, column=c, sticky="nsew")
					continue
				bg = self.CELL_BG
				if self.year == today_y and self.month == today_m and day == today_d:
					bg = self.CELL_BG_TODAY
				if day == self.day:
					bg = self.CELL_BG_SELECTED
				tk.Button(
					self.gridframe,
					text=str(day),
					width=self.CELL_WIDTH,
					height=self.CELL_HEIGHT,
					bd=self.BORDER_WIDTH,
					relief="solid",
					bg=bg,
					activebackground=self.ACTIVE_BG,
					command=lambda d=day: self.select_day(d),
				).grid(row=r, column=c, sticky="nsew")
	
	
	def select_day(self, day):
		if day == 0:
			return
		self.set_date(self.year, self.month, day)
		self._close_popup()
	
	
	def prev_month(self):
		self.month -= 1
		if self.month == 0:
			self.month = 12
			self.year -= 1
		self.draw_calendar()
	
	
	def next_month(self):
		self.month += 1
		if self.month == 13:
			self.month = 1
			self.year += 1
		self.draw_calendar()
	
	
	def prev_year(self):
		if self.year <= 2000:
			return
		self.year -= 1
		self.draw_calendar()
	
	
	def next_year(self):
		if self.year >= 2100:
			return
		self.year += 1
		self.draw_calendar()
