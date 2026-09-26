import { useState } from "react";
import { Check, ChevronRight, Palette } from "lucide-react";
import { themes, type useTheme } from "../theme";
import { Modal } from "./Modal";

export function ThemePicker({
  theme,
  changeTheme,
  storageError,
  windowError,
}: ReturnType<typeof useTheme>) {
  const [open, setOpen] = useState(false);
  const selected = themes.find((item) => item.id === theme)!;
  return (
    <>
      <button
        className="appearance-button"
        aria-label="外观皮肤"
        title={"外观皮肤 · " + selected.name}
        aria-haspopup="dialog"
        onClick={() => setOpen(true)}
      >
        <Palette size={18} />
        <span className="appearance-button-copy">
          <span>外观皮肤</span>
          <small>{selected.name}</small>
        </span>
        <ChevronRight className="appearance-chevron" size={14} />
      </button>
      {open && (
        <Modal
          title="外观皮肤"
          subtitle="选择喜欢的配色，立即生效，自动记住你的偏好。"
          onClose={() => setOpen(false)}
        >
          <div className="appearance-body">
            <fieldset className="theme-options">
              <legend className="visually-hidden">选择界面皮肤</legend>
              {themes.map((item) => (
                <label
                  key={item.id}
                  className={
                    "theme-card" + (theme === item.id ? " selected" : "")
                  }
                >
                  <input
                    className="visually-hidden"
                    type="radio"
                    name="appearance-theme"
                    value={item.id}
                    checked={theme === item.id}
                    aria-label={item.name}
                    aria-describedby={"theme-description-" + item.id}
                    onChange={() => changeTheme(item.id)}
                  />
                  <span
                    className="theme-preview"
                    data-theme={item.id}
                    aria-hidden="true"
                  >
                    <span className="theme-preview-sidebar">
                      <i />
                      <i />
                      <i />
                      <i />
                    </span>
                    <span className="theme-preview-main">
                      <span className="theme-preview-header">
                        <i />
                        <b />
                      </span>
                      <span className="theme-preview-metrics">
                        <i />
                        <i />
                        <i />
                      </span>
                      <span className="theme-preview-content">
                        <span className="theme-preview-list">
                          <i />
                          <i />
                          <i />
                        </span>
                        <span className="theme-preview-chart">
                          <i />
                          <i />
                          <i />
                        </span>
                      </span>
                    </span>
                  </span>
                  <span className="theme-card-heading">
                    <strong>{item.name}</strong>
                    <span className="theme-card-check" aria-hidden="true">
                      {theme === item.id && (
                        <Check size={13} strokeWidth={2.6} />
                      )}
                    </span>
                  </span>
                  <span
                    className="theme-card-description"
                    id={"theme-description-" + item.id}
                  >
                    {item.description}
                  </span>
                </label>
              ))}
            </fieldset>
            {(storageError || windowError) && (
              <p className="theme-error" role="alert">
                {[storageError, windowError].filter(Boolean).join(" ")}
              </p>
            )}
          </div>
          <div className="appearance-footer">
            <p role="status">
              <Check size={15} />
              当前使用：{selected.name}
            </p>
            <button className="primary" onClick={() => setOpen(false)}>
              完成
            </button>
          </div>
        </Modal>
      )}
    </>
  );
}
