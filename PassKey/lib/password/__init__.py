try:
    from typing import Iterator, Optional, Union, Self
except ImportError:
    pass

import json

class Account:
    def __init__(self, json_data: dict):
        self.name = json_data.get("Name")
        self.login_id = json_data.get("LoginId")
        self.password = json_data.get("Password")
        
    def to_dict(self) -> dict:
        return {
            "Name": self.name,
            "LoginId": self.login_id,
            "Password": self.password
        }

class Accounts:
    def __init__(self, json_data: dict):
        self._accounts = []
        for account in json_data.get("Accounts", []):
            self.append(account)
            
    @staticmethod
    def load_from_file(file_path: str) -> Self:
        try:
            with open(file_path, 'r') as file:
                return Accounts(json.load(file))
        except Exception as ex:
            print(f"Failed to load data from {file_path}: {ex}")
            return Accounts({})
        
    def save_to_file(self, file_path: str) -> None:
        try:
            with open(file_path, 'w') as file:
                json.dump(self.to_dict(), file)
        except Exception as ex:
            print(f"Failed to save data to {file_path}: {ex}")
        
    def append(self, account: Union[dict, Self]) -> None:
        if isinstance(account, dict):
            self._accounts.append(Account(account))
        elif isinstance(account, Account):
            self._accounts.append(account)
        else:
            raise TypeError("Argument must be either a dictionary or an Account instance")
        
    def pop(self, index: int = -1) -> Optional[Self]:
        return self._accounts.pop(index)
        
    def remove(self, account: Self) -> None:
        self._accounts.remove(account)
    
    @property
    def first(self) -> Optional[Account]:
        return self._accounts[0] if self._accounts else None
    
    def __getitem__(self, index: int) -> Account:
        return self._accounts[index]
    
    def __setitem__(self, index: int, value: Union[dict, Account]) -> None:
        if isinstance(value, dict):
            self._accounts[index] = Account(value)
        elif isinstance(value, Account):
            self._accounts[index] = value
        else:
            raise TypeError("Argument must be either a dictionary or an Account instance")
    
    def __len__(self) -> int:
        return len(self._accounts)
    
    def __iter__(self) -> Iterator[Account]:
        return iter(self._accounts)
        
    def to_dict(self) -> list[dict]:
        return [account.to_dict() for account in self._accounts]
