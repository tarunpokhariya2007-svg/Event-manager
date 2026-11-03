import { UserSettings } from '../types/settings';
import { initialUserSettings } from '../data/initialData';
import { clearLocalExtras } from './backendDataService';

const SETTINGS_KEY = 'chronoplan_settings_v1';

export const storageService = {
  loadSettings(): UserSettings {
    try {
      const data = localStorage.getItem(SETTINGS_KEY);
      if (data) {
        return { ...initialUserSettings, ...JSON.parse(data) };
      }
    } catch (e) {
      console.warn('Failed to load settings from localStorage', e);
    }
    return initialUserSettings;
  },

  saveSettings(settings: UserSettings): void {
    try {
      localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings));
    } catch (e) {
      console.warn('Failed to save settings to localStorage', e);
    }
  },

  resetSettings(): UserSettings {
    localStorage.removeItem(SETTINGS_KEY);
    clearLocalExtras();
    return initialUserSettings;
  }
};
