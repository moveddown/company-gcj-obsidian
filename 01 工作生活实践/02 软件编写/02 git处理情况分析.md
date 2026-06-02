# 一、提交情况

### 1. 提交后回到了main分支，之前一个分支的提交不见了

代码当时是main分支，我想要添加一个新功能，也就是加一个开机动画。然后创建了分支是为了实现开机动画。先提交了开机动画1，然后基于这个动画1又改了，提交了动画1+。这个时候动画要大变，所以在动画1+这个路径我从新改了提交了动画2，这个时候动画2已经提交并且上传到github了。 但是后面要求变了，又变了要在动画1+修改。那么我就直接切了分支，发现切换不了。我就Add Tag，然后在checkout Branch 就到动画1+了。这个时候，我开始基于这个修改了一些bug。修改完毕后提交为动画1+bug，我想要回到mian分支，然后结合一下当前分支。然后发现了切到main分支后，就看不到动画1+bug这个分支了。


---

#### 为什么会发生这次“提交丢失”

这次并不是代码真的被 Git 删除了，而是提交变成了“没有分支指向的提交”。

当时的真实情况大概是：

1. 最开始在 `main` 上创建了开机动画分支。
2. 在这个分支上先后提交了：`动画1`、`动画1+`、`动画2`。
3. `动画2` 已经提交并上传到 GitHub。
4. 后来需求变了，又想回到 `动画1+` 这个历史位置继续改。
5. 这时使用了 `Add Tag`，然后 checkout 到这个 tag 或某个历史提交位置。
6. 在这个位置继续修改并提交了 `动画1+bug`。
7. 再切回 `main` 后，发现看不到 `动画1+bug` 这个提交/分支。

关键原因：

- `tag` 只是一个固定标记，不是分支。
- `tag` 不会随着新提交向前移动。
- 如果 checkout 到 tag 或某个历史 commit 后直接提交，Git 会进入 `detached HEAD` 状态。
- 在 `detached HEAD` 状态下提交出来的 commit 是存在的，但没有任何分支名指向它。
- 一旦切回 `main`，普通分支图里就看不到这个提交，看起来就像“丢了”。

这类提交一般还能从 `git reflog` 里找回来，但如果时间太久，被 Git 垃圾回收清理，就可能真的找不回来了。

#### 以后怎么避免

核心原则：

> 想回到历史版本继续开发时，不要直接在 tag 或历史 commit 上改；一定要先从那个位置创建新分支。

#####  1. 只是想看看旧版本

如果只是临时看一下 `长鱼游过的效果` 这个 tag 对应的代码，可以这样：

```bash
git switch --detach "长鱼游过的效果"
```

看完回到 `main`：

```bash
git switch main
```

注意：这种情况下不要直接改完提交。

##### 2. 想基于旧版本继续改

如果要基于 `长鱼游过的效果` 继续改，正确做法是先创建新分支：

```bash
git switch -c fix-long-fish-animation "长鱼游过的效果"
```

然后再修改、提交：

```bash
git add .
git commit -m "fix: 基于长鱼游过效果修改动画"
```

如果要上传到 GitHub：

```bash
git push -u origin fix-long-fish-animation
```

如果后面要合并回 `main`：

```bash
git switch main
git merge fix-long-fish-animation
git push
```

##### 3. 在图形界面里应该怎么理解

右击 tag 时，一般只会看到：

- `Delete Tag`
- `Push Tag`
- `Copy Tag Name`

这是正常的，因为 tag 不是分支。

如果图形界面没有“切换分支”的选项，就不要强行 checkout tag 后直接改。应该找类似下面的选项：

- `Create Branch from Tag`
- `Create Branch from Commit`
- `Create Branch Here`

如果界面找不到，就直接用命令最稳：

```bash
git switch -c 新分支名 tag名或commit_id
```

#### 已经发生了怎么解决

如果已经在 tag 或历史 commit 上提交了，切回 `main` 后发现提交不见了，可以这样找回。

##### 1. 先查 reflog

```bash
git reflog
```

找到刚刚丢失的提交，例如这次是：

```text
fe9d28e fix : 处理4G超时1min后应该抛弃数据，还有音量的界面显示
```

##### 2. 先创建保护分支

先给这个悬空提交创建一个分支，防止以后被清理：

```bash
git branch recover-fe9d28e fe9d28e
```

或者直接切过去：

```bash
git switch -c recover-fe9d28e fe9d28e
```

##### 3. 如果要恢复到 main

切回 `main`：

```bash
git switch main
```

把这个提交应用回来：

```bash
git cherry-pick fe9d28e
```

这次实际恢复后，原来的提交是：

```text
fe9d28ea12c2218af083ebbd4b12aadbf79eee4a
```

恢复到 `main` 后生成的新提交是：

```text
902bd8a fix : 处理4G超时1min后应该抛弃数据，还有音量的界面显示
```

#### 一句话记忆

> tag 只负责标记历史位置，branch 才负责承接后续开发。

所以：

- 看旧版本：可以 checkout tag。
- 改旧版本：必须从 tag/commit 创建新分支。
- 已经丢了：用 `git reflog` 找提交，再 `git branch` 保护，最后 `git cherry-pick` 回目标分支。