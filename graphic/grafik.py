import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("logs/max30100_2025-09-29_16-38-17.csv")
print(df.head())

fig, axs = plt.subplots(4, 1, figsize=(12, 8), sharex=True)

axs[0].plot(df["raw_ir"], label="Raw IR")
axs[0].plot(df["raw_red"], label="Raw RED")
axs[0].legend(); axs[0].set_ylabel("Raw")

axs[1].plot(df["ac_ir"], label="AC IR")
axs[1].plot(df["ac_red"], label="AC RED")
axs[1].legend(); axs[1].set_ylabel("AC")

axs[2].plot(df["md_ir"], label="MeanDiff IR")
axs[2].plot(df["md_red"], label="MeanDiff RED")
axs[2].legend(); axs[2].set_ylabel("MeanDiff")

axs[3].plot(df["lp_ir"], label="LPBWF IR")
axs[3].plot(df["lp_red"], label="LPBWF RED")
axs[3].legend(); axs[3].set_ylabel("LPBWF")
axs[3].set_xlabel("Samples")

plt.tight_layout()
plt.show()
