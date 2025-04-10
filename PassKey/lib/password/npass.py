import board
import time
import digitalio
import json
import usb_hid
from adafruit_hid.keyboard import Keyboard
from adafruit_hid.keyboard_layout_us import KeyboardLayoutUS
from password.serial import Serial
from password.oled import Oled, TextAlignment
from password.button import Button
from password import Account, Accounts

class DataType:
    ADD_ACCOUNT = 0
    REMOVE_ACCOUNTS = 1
    EDIT_ACCOUNT = 2
    SWAP_ACCOUNTS = 3
    SAVE_ACCOUNTS = 4
    
class Npass:
    FILE_PATH = "data.json"
    SPLASH_DELAY = 0.5
    MAX_NAME_LENGTH = 15
    
    def __init__(self):
        Serial.init(self._on_serial_data_received, self._on_serial_connected)
        
        self._accounts = Accounts.load_from_file(self.FILE_PATH)
        self._selected_account_index = 0
        self._unsaved_changes = False
        
        self._kbd = Keyboard(usb_hid.devices)
        self._kbd_layout = KeyboardLayoutUS(self._kbd)
        
        self._left_btn = Button(board.GP15, digitalio.Pull.UP)
        self._middle_btn = Button(board.GP26, digitalio.Pull.UP)
        self._right_btn = Button(board.GP14, digitalio.Pull.UP)
        
        self._oled = Oled(board.GP1, board.GP0)
        
        # Splash Screen
        self._oled.clear()
        self._oled.add_text("WPass", alignment=TextAlignment.CENTER)
        time.sleep(self.SPLASH_DELAY)
        self._oled.clear()
        
        # Lock Screen
        self._lock_screen()
        
        self._left_arrow_label = self._oled.add_text("<", alignment=TextAlignment.MIDDLE_LEFT, hidden=True)
        self._right_arrow_label = self._oled.add_text(">", alignment=TextAlignment.MIDDLE_RIGHT, hidden=True)
        self._unsaved_changes_label = self._oled.add_text("[Unsaved Changes]", alignment=TextAlignment.BOTTOM_CENTER, hidden=True)
        self._account_index_label = self._oled.add_text("", alignment=TextAlignment.TOP_CENTER)
        self._account_name_label = self._oled.add_scrolling_text("", max_characters=self.MAX_NAME_LENGTH, alignment=TextAlignment.CENTER)
        self._refresh_oled()
        
    def _lock_screen(self) -> None:
        pattern = [
            { "button": 0, "press_count": 1 },
            { "button": 1, "press_count": 2 }
        ]
        
        lock_status_label = self._oled.add_text("Locked", alignment = TextAlignment.CENTER)
        
        current_index = 0
        press_count = 0
        
        buttons = {
                0: self._left_btn,
                1: self._middle_btn,
                2: self._right_btn
            }
        
        while True:
            self._left_btn.update()
            self._middle_btn.update()
            self._right_btn.update()
            
            expected_btn_index = pattern[current_index].get("button")
            target_press_count = pattern[current_index].get("press_count")
            
            if buttons.get(expected_btn_index).pressed:
                press_count += 1
                if press_count == target_press_count:
                    current_index += 1
                    press_count = 0
                    if current_index >= len(pattern):
                        print("Unlocked")
                        self._oled.remove(lock_status_label)
                        break
            else:
                # Reset the pattern only if an unexpected button is pressed (and not the expected button)
                if any(btn.pressed for i, btn in buttons.items() if i != expected_btn_index):
                    current_index = 0
                    press_count = 0
                    # lock_status_label.text = "Invalid Pattern"
                    # time.sleep(0.5)
                    # lock_status_label.text = "Locked"

    @property
    def selected_account_index(self) -> int:
        return self._selected_account_index
    
    @selected_account_index.setter
    def selected_account_index(self, index: int) -> None:
        if not self._accounts:
            raise ValueError("No accounts available to select.")
        if 0 <= index < len(self._accounts):
            self._selected_account_index = index
            self._refresh_oled()
        else:
            raise IndexError(f"Index {index} out of range (0 to {len(self._accounts) - 1}).")
        
    def start(self) -> None:
        while True:
            self._update()
            
    def _update(self) -> None:
        Serial.update()
        self._oled.update()
        self._left_btn.update()
        self._middle_btn.update()
        self._right_btn.update()
        self._handle_input()    
        
    def _handle_input(self) -> None:
        if self._middle_btn.long_press:
            self._oled.toggle_power()
            
        if not self._oled.is_awake:
            return

        if self._left_btn.pressed:
            self.selected_account_index = (self.selected_account_index - 1) % len(self._accounts)
        
        if self._right_btn.pressed:
            self.selected_account_index = (self.selected_account_index + 1) % len(self._accounts)
        
        if self._middle_btn.short_count == 1:
            self._kbd_layout.write(self._accounts[self.selected_account_index].login_id)
        elif self._middle_btn.short_count == 2:
            self._kbd_layout.write(self._accounts[self.selected_account_index].password)
            
    def _on_serial_connected(self) -> None:
        Serial.writeline(self._accounts.to_dict())
        
    def _on_serial_data_received(self, json_data: dict) -> None:
        data_type = json_data.get("DataType")
        if data_type is None:
            return
        
        handlers = {
            DataType.ADD_ACCOUNT: self._handle_add_account,
            DataType.REMOVE_ACCOUNTS: self._handle_remove_accounts,
            DataType.EDIT_ACCOUNT: self._handle_edit_account,
            DataType.SWAP_ACCOUNTS: self._handle_swap_accounts,
            DataType.SAVE_ACCOUNTS: self._handle_save_accounts
        }
        
        handler = handlers.get(data_type)
        if handler:
            handler(json_data)
            self._refresh_oled()
            
    def _handle_add_account(self, json_data: dict) -> None:
        account = json_data.get("Account")
        if account:
            self._accounts.append(account)
            self._unsaved_changes = True
            
    def _handle_remove_accounts(self, json_data: dict) -> None:
        indexes = json_data.get("Indexes")
        if indexes:
            for index in sorted(indexes, reverse=True):
                if 0 <= index < len(self._accounts):
                    self._accounts.pop(index)
                    
                    if self._selected_account_index >= len(self._accounts):
                        self._selected_account_index = max(0, len(self._accounts) - 1)
                    elif index <= self._selected_account_index:
                        self._selected_account_index = max(0, self._selected_account_index - 1)
            self._unsaved_changes = True
            
    def _handle_edit_account(self, json_data: dict) -> None:
        index = json_data.get("Index")
        account = json_data.get("Account")
        if index is not None and account:
            self._accounts[index] = account
            self._unsaved_changes = True
            
    def _handle_swap_accounts(self, json_data: dict) -> None:
        from_index = json_data.get("FromIndex")
        to_index = json_data.get("ToIndex")
        if from_index is not None and to_index is not None:
            self._accounts[from_index], self._accounts[to_index] = self._accounts[to_index], self._accounts[from_index]
            self._unsaved_changes = True
    
    def _handle_save_accounts(self, _) -> None:
        self._accounts.save_to_file(self.FILE_PATH)
        self._unsaved_changes = False
        
    def _refresh_oled(self) -> None:
        if self._accounts:
            self._account_index_label.text = f"Account: {self.selected_account_index + 1}/{len(self._accounts)}"
            self._account_name_label.text = self._accounts[self.selected_account_index].name
            self._left_arrow_label.hidden = False
            self._right_arrow_label.hidden = False
        else:
            self._account_index_label.text = ""
            self._account_name_label.text = "No Accounts"
            self._left_arrow_label.hidden = True
            self._right_arrow_label.hidden = True
            
        self._unsaved_changes_label.hidden = not self._unsaved_changes