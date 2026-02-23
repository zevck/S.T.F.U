declare global {
  interface Window {
    SKSE_API?: {
      call: (eventName: string, ...args: any[]) => void;
    };
  }
}

export {};
